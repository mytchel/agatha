#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <log.h>
#include <pool.h>
#include <proc0.h>
#include <arm/mmu.h>

#define LOG 1
#if !LOG
#define log(X, ...) {}
#endif

struct span {
	uint32_t pa, va, len;
	struct span **holder, *next;
};

struct l1_span {
	uint32_t pa, va, len;

	struct span *free, *mapped;

	struct l1_span **holder, *next;
};

#define l1_span_initial_size \
		(sizeof(struct pool_frame) + \
		 (sizeof(struct l1_span) + \
			sizeof(struct pool_obj)) * 5)

static uint8_t l1_span_pool_initial[l1_span_initial_size];

#define span_initial_size \
		(sizeof(struct pool_frame) + \
		 (sizeof(struct span) + \
			sizeof(struct pool_obj)) * 8)

static uint8_t span_pool_initial[span_initial_size];

static struct pool l1_span_pool;
static struct pool span_pool;

static bool initialized = false;

static struct l1_span 
	*l1_free = nil, 
	*l1_mapped = nil;

extern uint32_t *_text_start;
extern uint32_t *_text_end;
extern uint32_t *_data_start;
extern uint32_t *_data_end;

int
addr_unmap(size_t va, size_t len);

int
addr_map(size_t pa, size_t va, size_t len, int flags);

int
addr_map_l2s(size_t pa, size_t va, size_t table_len);

	void
addr_init(void)
{
	struct span *m, *f;

	pool_init(&l1_span_pool, sizeof(struct l1_span));
	pool_init(&span_pool, sizeof(struct span));

	pool_load(&l1_span_pool, 
			&l1_span_pool_initial,
			sizeof(l1_span_pool_initial));

	pool_load(&span_pool, 
			&span_pool_initial,
			sizeof(span_pool_initial));

	l1_mapped = pool_alloc(&l1_span_pool);
	l1_free = pool_alloc(&l1_span_pool);

	/* TODO: ignores space before text start (this 
		 is currently where proc0 maps the stack
		 and the l2 table which we could use to
		 find more of this information) */

	l1_mapped->holder = &l1_mapped;
	l1_mapped->next = nil;
	l1_mapped->pa = nil;
	l1_mapped->va = TABLE_ALIGN_DN(&_text_start);
	l1_mapped->len = TABLE_ALIGN(&_data_end) - l1_mapped->va;

	m = pool_alloc(&span_pool);
	l1_mapped->mapped = m;
	m->holder = &l1_mapped->mapped;
	m->va = PAGE_ALIGN_DN(&_text_start);
	m->len = PAGE_ALIGN(&_data_end) - m->va;
	m->pa = nil;
	m->next = nil;

	f = pool_alloc(&span_pool);
	l1_mapped->free = f;
	f->holder = &l1_mapped->free;
	f->va = m->va + m->len;
	f->len = l1_mapped->va + l1_mapped->len - f->va;
	f->pa = nil;
	f->next = nil;

	l1_free->holder = &l1_free;
	l1_free->va = l1_mapped->va + l1_mapped->len;
	l1_free->len = 0xff000000 - l1_free->va;
	l1_free->pa = nil;
	l1_free->free = nil;
	l1_free->mapped = nil;
	l1_free->next = nil;

	initialized = true;
}

void
take_add_span(struct span *s, struct span **nl)
{
	*s->holder = s->next;
	if (s->next) s->next->holder = s->holder;

	while (*nl != nil) {
		if (s->va < (*nl)->va) {
			break;
		}

		nl = &(*nl)->next;
	}
	
	s->next = *nl;
	if (s->next) s->next->holder = &s->next;

	*nl = s;
	s->holder = nl;
}

int
split_span(struct span *s, size_t len)
{
	struct span *n;

	log(LOG_INFO, "splitting span 0x%x 0x%x to len 0x%x",
			s->va, s->len, len);

	n = pool_alloc(&span_pool);
	if (n == nil) {
		log(LOG_INFO, "out of spans");
		return ERR;
	}

	n->pa = s->pa + len;
	n->va = s->va + len;
	n->len = s->len - len;

	n->next = s->next;
	if (n->next) n->next->holder = &n->next;

	s->next = n;
	n->holder = &s->next;

	s->len = len;

	log(LOG_INFO, "now have 0x%x 0x%x and 0x%x 0x%x",
			s->va, s->len, n->va, n->len);

	return OK;
}

void
take_add_l1_span(struct l1_span *s, struct l1_span **nl)
{
	*s->holder = s->next;
	if (s->next) s->next->holder = s->holder;

	while (*nl != nil) {
		if (s->va < (*nl)->va) {
			break;
		}

		nl = &(*nl)->next;
	}
	
	s->next = *nl;
	if (s->next) s->next->holder = &s->next;

	*nl = s;
	s->holder = nl;
}

int
split_l1_span(struct l1_span *s, size_t len)
{
	struct l1_span *n;

	n = pool_alloc(&l1_span_pool);
	if (n == nil) {
		log(LOG_INFO, "out of l1_spans");
		return ERR;
	}

	if (s->pa != nil) {
		n->pa = s->pa + len;
	} else {
		n->pa = nil;
	}

	n->va = s->va + len;
	n->len = s->len - len;

	n->next = s->next;
	if (n->next) n->next->holder = &n->next;

	s->next = n;
	n->holder = &s->next;

	s->len = len;

	n->mapped = nil;
	n->free = nil;

	return OK;
}

	void
dump_mappings(void)
{
	struct l1_span *l;
	struct span *s;

	log(LOG_INFO, "currently mapped:");

	for (l = l1_mapped; l != nil; l = l->next) {
		log(LOG_INFO, "l1 0x%x to 0x%x", l->va, l->va + l->len);
		for (s = l->mapped; s != nil; s = s->next) {
			log(LOG_INFO, "m  0x%x to 0x%x -> 0x%x", s->va, s->va + s->len, s->pa);
		}

		for (s = l->free; s != nil; s = s->next) {
			log(LOG_INFO, "f  0x%x to 0x%x", s->va, s->va + s->len);
		}
	}

	log(LOG_INFO, "currently free:");

	for (l = l1_free; l != nil; l = l->next) {
		log(LOG_INFO, "l1 0x%x to 0x%x", l->va, l->va + l->len);
	}
}

struct span *
map_free_addr(size_t pa, size_t len, int flags)
{
	struct l1_span *l, *fl;
	struct span *s, *fs;
	size_t l1_pa, tlen;
	int r;

	log(LOG_INFO, "find addr for 0x%x bytes", len);

	dump_mappings();

	fs = nil;
	fl = nil;
	for (l = l1_mapped; l != nil; l = l->next) {
		for (s = l->free; s != nil; s = s->next) {
			if (len <= s->len && (fs == nil || s->len < fs->len)) {
				fl = l;
				fs = s;
			}
		}
	}

	if (fs != nil) {
		if (len < fs->len) {
			if ((r = split_span(fs, len)) != OK) {
				return nil;
			}
		}

		take_add_span(fs, &(fl)->mapped);

		log(LOG_INFO, "giving 0x%x", fs->va);

		fs->pa = pa;

		if ((r = addr_map(pa, fs->va, len, flags)) != OK) {
			/* TODO: put span into free */
			log(LOG_INFO, "addr map failed %i for 0x%x -> 0x%x 0x%x", r, pa, fs->va, len);
			return nil;
		}

		return fs;
	}

	log(LOG_INFO, "need a new l1 span");

	/* Need new l1 span */
	fl = nil;
	for (l = l1_free; l != nil; l = l->next) {
		if (l->len > len && (fl == nil || l->len < fl->len)) {
			fl = l;
		}
	}

	if (fl == nil) {
		return nil;
	}

	fs = pool_alloc(&span_pool);
	if (s == nil) {
		return nil;
	}

	tlen = (TABLE_ALIGN(len) >> TABLE_SHIFT) * TABLE_SIZE;
	l1_pa = request_memory(tlen);
	if (l1_pa == nil) {
		pool_free(&span_pool, fs);
		return nil;
	}

	r = addr_map_l2s(l1_pa, fl->va, tlen);
	if (r != OK) {
		release_addr(l1_pa, tlen);
		pool_free(&span_pool, fs);
		return nil;
	}

	if (fl->len > TABLE_ALIGN(len)) {
		if ((r = split_l1_span(fl, TABLE_ALIGN(len))) != OK) {
			return nil;
		}
	}

	fl->pa = l1_pa;
	take_add_l1_span(fl, &l1_mapped);

	fs->pa = nil;
	fs->va = fl->va;
	fs->len = fl->len;
	fs->next = nil;
	fs->holder = &fl->free;
	fl->free = s;

	if (len < fs->len) {
		if ((r = split_span(fs, len)) != OK) {
			return nil;
		}
	}

	take_add_span(fs, &fl->mapped);

	log(LOG_INFO, "giving 0x%x", fs->va);

	fs->pa = pa;

	if ((r = addr_map(pa, fs->va, len, flags)) != OK) {
		/* TODO: put span into free */
		log(LOG_INFO, "addr map failed %i for 0x%x -> 0x%x 0x%x", r, pa, fs->va, len);
		return nil;
	}

	return fs;
}

	static bool
grow_pool(struct pool *p)
{
	size_t pa, len;
	void *va;

	len = PAGE_ALIGN(sizeof(struct pool_frame) + pool_obj_size(p) * 64);

	pa = request_memory(len);
	if (pa == nil) {
		return false;
	}

	va = map_addr(pa, len, MAP_RW|MAP_MEM);
	if (va == nil) {
		release_addr(pa, len);
		return false;
	}

	return pool_load(p, va, len) == OK;
}

	static bool
check_pools(void)
{
	static bool checking = false;	
	bool r = true;

	if (checking) {
		return true;
	}

	if (pool_n_free(&l1_span_pool) < 3) {
		log(LOG_INFO, "growing l1 span pool");
		checking = true;
		r = grow_pool(&l1_span_pool);
		log(LOG_INFO, "pool grown ? %i", r);
		if (!r) exit(0xff00);
		checking = false;
	}

	if (r && pool_n_free(&span_pool) < 4) {
		log(LOG_INFO, "growing span pool");
		checking = true;
		r = grow_pool(&span_pool);
		log(LOG_INFO, "pool grown ? %i", r);
		if (!r) exit(0xff01);
		checking = false;
	}

	return r;
}

	void *
map_addr(size_t pa, size_t len, int flags)
{
	struct span *s;

	log(LOG_INFO, "find map for 0x%x 0x%x", pa, len);

	if (!initialized)
		addr_init();

	if (PAGE_ALIGN(pa) != pa) {
		log(LOG_WARNING, "0x%x is not aligned", pa);
		return nil;
	}

	if (!check_pools()) {
		return nil;
	}

	len = PAGE_ALIGN(len);

	s = map_free_addr(pa, len, flags);
	if (s == nil) {
		return nil;
	}

	return (void *) s->va;
}

	int
unmap_addr(void *addr, size_t len)
{
	size_t va = (size_t) addr;
	struct l1_span *l;
	struct span *s;

	log(LOG_INFO, "unmap_addr 0x%x 0x%x", addr, len);

	dump_mappings();

	if (addr_unmap(va, len) != OK) {
		exit(0x123456);
		return ERR;
	}

	log(LOG_INFO, "unmap worked, continue");

	if (!initialized)
		return ERR;

	for (l = l1_mapped; l != nil; l = l->next) {
		log(LOG_INFO, "is it in 0x%x 0x%x?", l->va, l->len);
		if (l->va <= va && va + len <= l->va + l->len) {
			log(LOG_INFO, "yes");
			break;
		}
	}

	if (l == nil) {
		log(LOG_INFO, "error unmapping, l1 not mapped");

		exit(0x1236);
		return ERR;
	}

	log(LOG_INFO, "find span in 0x%x 0x%x?", l->va, l->len);
	for (s = l->mapped; s != nil; s = s->next) {
		log(LOG_INFO, "is it in 0x%x 0x%x?", s->va, s->len);
		if (s->va <= va && va + len <= s->va + s->len) {
			log(LOG_INFO, "yes");
			break;
		}
	}

	if (s == nil) {
		log(LOG_INFO, "error unmapping, pages not mapped");
		exit(0x1235);
		return ERR;
	}

	if (s->va != va || s->len != len) {
		log(LOG_INFO, "error unmapping, cannot unmap across mappings");
		/* TODO: support unmapping parts of mapped space */
		exit(0x1234);
		return ERR;
	}

	/* Unmap */
	s->pa = nil;

	take_add_span(s, &l->free);

	return OK;
}

