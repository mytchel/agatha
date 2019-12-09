#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <sysobj.h>
#include <c.h>
#include <pool.h>
#include <log.h>
#include <arm/mmu.h>

/*#define log(X, ...) {}*/

typedef struct kcap kcap_t;
typedef struct kobj kobj_t;
typedef struct kcap_list kcap_list_t;

struct kcap {
	int cid;
	kobj_t *obj;
	kcap_t *next;
};

struct kobj {
	int cid;
	bool claimed;
	int type;
	size_t len;
	kobj_t *next;
	kobj_t *from;
};

#define cap_list_n  255
struct kcap_list {
	uint32_t caps[255/32];
};

static kobj_t kobj_pool_initial[64] = { 0 };

static struct pool kobj_pool = { 0 };
static kobj_t *kobjs = nil;

static kcap_list_t kcap_list_initial = { 0 };

static bool ready = false;

static void
ksys_init(void)
{
	if (ready) return;

	kobjs = nil;

	if (pool_init(&kobj_pool, sizeof(kobj_t)) != OK) {
		exit(1);
	}

	if (pool_load(&kobj_pool, kobj_pool_initial, sizeof(kobj_pool_initial)) != OK) {
		exit(1);
	}

	kcap_list_t *i;
	i = &kcap_list_initial;
	memset(i->caps, 0, sizeof(i->caps));
	i->caps[0] |= (1<<0) | (1<<1) | (1<<2) | (1<<3);

	ready = true;
}

static bool
grow_pool(void)
{
	size_t len;
	void *va;
	int fid;

	log(LOG_INFO, "grow kobj pool");

	len = PAGE_ALIGN(sizeof(struct pool_frame) 
		+ pool_obj_size(&kobj_pool) * 64);

	fid = request_memory(len, 0x1000);
	if (fid < 0) {
		return false;
	}

	va = frame_map_anywhere(fid, len);
	if (va == nil) {
		release_memory(fid);
		return false;
	}

	if (pool_load(&kobj_pool, va, len) != OK) {
		log(LOG_WARNING, "pool load failed");
		log(LOG_WARNING, "TODO: unmap and free");
		return false;
	}

	log(LOG_INFO, "kobj pool grown");
	return true;
}

static kobj_t *
kobj_pool_alloc(void)
{
	static bool checking = false;

	if (!checking) {
		if (pool_n_free(&kobj_pool) < 5) {
			checking = true;
			grow_pool();
			checking = false;
		}
	}

	return pool_alloc(&kobj_pool);
}

int
kcap_alloc(void)
{
	kcap_list_t *c;
	size_t o;

	log(LOG_INFO, "kcap alloc");

	ksys_init();

	c = &kcap_list_initial;
	for (o = 0; o < cap_list_n; o++) {
		if (!(c->caps[o/32] & (1 << (o % 32)))) {
			c->caps[o/32] |= 1 << (o % 32);

			log(LOG_INFO, "kcap found free 0x%x", o<<12);

			return o << 12;
		}
	}

	log(LOG_WARNING, "need more");

	return ERR;
}

void
kcap_free(int cid)
{

}

static kobj_t *
kobj_alloc_new(size_t len)
{
	int cid, fid;
	kobj_t *o;

	log(LOG_WARNING, "kobj alloc new 0x%x", len);

	if (len < 0x1000) 
		len = 0x1000;

	fid = request_memory(len, 0x1000);
	if (fid < 0) {
		log(LOG_WARNING, "request mem failed");
		return nil;
	}

	cid = kcap_alloc();
	if (cid < 0) {
		log(LOG_WARNING, "kcap alloc failed");
		return nil;
	}
	
	if (obj_create(fid, cid) != OK) {
		log(LOG_WARNING, "obj create failed");
		return nil;
	}

	kcap_free(fid);

	o = kobj_pool_alloc();
	if (o == nil) {
		log(LOG_WARNING, "kobj pool empty");
		return nil;
	}

	o->cid = cid;
	o->claimed = false;
	o->type = OBJ_untyped;
	o->len = len;
	o->next = kobjs;
	o->from = o;
	kobjs = o;

	return o;
}

bool
kobj_split(kobj_t *o)
{
	kobj_t *n;
	int nid;

	log(LOG_INFO, "kobj split 0x%x len 0x%x", o->cid, o->len);

	if (o->type != OBJ_untyped) {
		return false;
	} else if (o->len < 4) {
		return false;
	}

	nid = kcap_alloc();
	if (nid < 0) {
		return false;
	}

	log(LOG_INFO, "next id 0x%x", nid);

	n = kobj_pool_alloc();
	if (n == nil) {
		log(LOG_INFO, "kobj pool empty");
		return false;
	}

	if (obj_split(o->cid, nid) != OK) {
		log(LOG_WARNING, "obj split failed");
		return false;
	}
	
	n->cid = nid;
	n->type = OBJ_untyped;
	n->len = o->len >> 1;
	n->next = o->next;
	n->from = o;

	o->len = o->len >> 1;
	o->next = n;

	log(LOG_INFO, "now have 0x%x and 0x%x with len %i %i",
		o->cid, n->cid, o->len, n->len);

	return true;
}

static size_t
kobj_type_size(int type, size_t n)
{
	switch (type) {
	case OBJ_untyped: return 0;
	case OBJ_endpoint: return sizeof(obj_endpoint_t);
	case OBJ_caplist: return sizeof(obj_caplist_t);
	case OBJ_proc: return sizeof(obj_proc_t);

	case OBJ_intr: return sizeof(obj_intr_t);
	case OBJ_frame: return sizeof(obj_frame_t);
	
	default: 
		return 0;
	}
}

int
kobj_add_untyped(int cid, size_t len)
{
	kobj_t *n;

	ksys_init();

	n = kobj_pool_alloc();
	if (n == nil) {
		log(LOG_INFO, "kobj pool empty");
		return ERR;
	}

	log(LOG_INFO, "have obj 0x%x", n);

	n->cid = cid;
	n->type = OBJ_untyped;
	n->len = len;
	n->from = n;

	n->next = kobjs;
	kobjs = n;

	return OK;
}

int
kobj_alloc(int type, size_t n)
{
	size_t l, len;
	kobj_t *o, *f;

	log(LOG_INFO, "kobj alloc %i %i", type, n);

	ksys_init();

	l = kobj_type_size(type, n);
	if (l == 0) {
		log(LOG_INFO, "type unknown");
		return ERR;
	}

	len = 1;
	while (len < l)
		len <<= 1;

	f = nil;
	for (o = kobjs; o != nil; o = o->next) {
		if (o->type != OBJ_untyped) continue;
		if (o->claimed) continue;
		if (len <= o->len) {
			if (f == nil || o->len < f->len) {
				f = o;
			}
		}
	}

	if (f == nil) {
		f = kobj_alloc_new(len);
		if (f == nil) {
			return ERR;
		}
	}

	f->claimed = true;

	while (len < f->len) {
		if (!kobj_split(f)) {
			return ERR;
		} 
	}

	if (obj_retype(f->cid, type, n) != OK) {
		return ERR;
	}

	f->type = type;

	return f->cid;
}

void
kobj_free(int cid)
{
	
}

