#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <sysobj.h>
#include <c.h>
#include <pool.h>

#define log(X, ...) {}

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
	int type;
	size_t len;
	kobj_t *next;
	kobj_t *from;
};

#define cap_list_n  255
struct kcap_list {
	kcap_list_t *next;
	int cid;
	int start_id;
	uint32_t caps[cap_list_n/32];
};

static int next_cap_list_cid = 2;

static struct pool kobj_pool;
static kobj_t *kobjs;

static struct pool kcap_pool;
static kcap_list_t *kcap_lists;

static bool ready = false;

static void
ksys_init(void)
{
	size_t pa, len;
	void *va;

	if (ready) return;

	kobjs = nil;
	kcap_lists = nil;

	if (pool_init(&kobj_pool, sizeof(kobj_t)) != OK) {
		exit(1);
	}

	len = 0x1000;

	pa = request_memory(len);
	if (pa == nil) {
		exit(1);
	}

	va = map_addr(pa, len, MAP_RW);
	if (va == nil) {
		exit(1);
	}

	if (pool_load(&kobj_pool, va, len) != OK) {
		exit(1);
	}

	if (pool_init(&kcap_pool, sizeof(kcap_list_t)) != OK) {
		exit(1);
	}

	len = 0x1000;

	pa = request_memory(len);
	if (pa == nil) {
		exit(1);
	}

	va = map_addr(pa, len, MAP_RW);
	if (va == nil) {
		exit(1);
	}

	if (pool_load(&kcap_pool, va, len) != OK) {
		exit(1);
	}

	ready = true;
}

int
kcap_get_more(void)
{
	int n = cap_list_n;
	size_t pa, len;
	kcap_list_t *c;

	log(0, "kcap get more");

	len = 0x1000;
	pa = request_memory(len);
	if (pa == nil) {
		return ERR;
	}

	if (next_cap_list_cid == 0) {
		return ERR;
	}

	int cid = next_cap_list_cid;
	next_cap_list_cid = 0;

	log(0, "kcap get more into id %i", cid);

	if (obj_create(cid, pa, len) != OK) {
		return ERR;
	}

	int r = obj_retype(cid, OBJ_caplist, n);
	if (r <= 0) {
		return ERR;
	}

	log(0, "kcap get more new start id %i", r);

	c = pool_alloc(&kcap_pool);
	if (c == nil) {
		return ERR;
	}

	memset(c, 0, sizeof(kcap_list_t));

	c->cid = cid;
	c->start_id = r;
	c->next = kcap_lists;
	kcap_lists = c;

	next_cap_list_cid = c->start_id;
	c->caps[0] |= 1 << 0;

	return OK;
}

int
kcap_alloc(void)
{
	kcap_list_t *c;
	size_t o;

	log(0, "kcap alloc");

	ksys_init();

	for (c = kcap_lists; c != nil; c = c->next) {
		log(0, "kcap check list %i", c->cid);
		for (o = 0; o < cap_list_n; o++) {
			if (!(c->caps[o/32] & (1 << (o % 32)))) {
				c->caps[o/32] |= 1 << (o % 32);
				log(0, "kcap found free %i + %i", 
					c->start_id, o);
				return c->start_id + o;
			}
		}
	}

	log(0, "need more");
	if (kcap_get_more() != OK) {
		return ERR;
	}

	return kcap_alloc();
}

void
kcap_free(int cid)
{

}

static kobj_t *
kobj_alloc_new(size_t len)
{
	kobj_t *o;
	size_t pa;
	int cid;

	if (len < 0x1000) 
		len = 0x1000;

	pa = request_memory(len);
	if (pa == nil) {
		return nil;
	}

	cid = kcap_alloc();
	if (cid < 0) {
		return nil;
	}
	
	if (obj_create(cid, pa, len) != OK) {
		return nil;
	}

	o = pool_alloc(&kobj_pool);
	if (o == nil) {
		return nil;
	}

	o->cid = cid;
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

	log(0, "kobj split %i len %i", o->cid, o->len);

	if (o->type != OBJ_untyped) {
		return false;
	} else if (o->len < 4) {
		return false;
	}

	nid = kcap_alloc();
	if (nid < 0) {
		return false;
	}

	log(0, "next id %i", nid);

	n = pool_alloc(&kobj_pool);
	if (n == nil) {
		log(0, "alloc failed");
		return false;
	}

	log(0, "have obj 0x%x", n);

	if (obj_split(o->cid, nid) != OK) {
		return false;
	}
	
	n->cid = nid;
	n->type = OBJ_untyped;
	n->len = o->len >> 1;
	n->next = o->next;
	n->from = o;

	o->len = o->len >> 1;
	o->next = n;

	log(0, "now have %i and %i with len %i %i",
		o->cid, n->cid, o->len, n->len);

	return true;
}

static size_t
kobj_type_size(int type, size_t n)
{
	switch (type) {
	case OBJ_untyped: return 0;
	case OBJ_endpoint: return sizeof(obj_endpoint_t);
	case OBJ_caplist: return sizeof(obj_caplist_t) + sizeof(cap_t) * n;
	case OBJ_proc: return sizeof(obj_proc_t);

	case OBJ_intr: return sizeof(obj_intr_t);
	
	default: 
		return 0;
	}
}

int
kobj_alloc(int type, size_t n)
{
	size_t l, len;
	kobj_t *o, *f;

	log(0, "kobj alloc %i %i", type, n);

	ksys_init();

	l = kobj_type_size(type, n);
	if (l == 0) {
		return ERR;
	}

	len = 1;
	while (len < l)
		len <<= 1;

	log(0, "kobj alloc size %i / %i", l, len);

	f = nil;
	for (o = kobjs; o != nil; o = o->next) {
		if (o->type != OBJ_untyped) continue;
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

	log(0, "kobj alloc now split %i to len %i",
		f->len, len);

	while (len < f->len) {
		if (!kobj_split(f)) {
			return ERR;
		} 
	}

	log(0, "do retype with f 0x%x / %i", f, f->cid);

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

