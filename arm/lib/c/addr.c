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
			sizeof(struct pool_obj)) * 6)

static uint8_t l1_span_pool_initial[l1_span_initial_size];

#define span_initial_size \
		(sizeof(struct pool_frame) + \
		 (sizeof(struct span) + \
			sizeof(struct pool_obj)) * 16)

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
	l1_free->len = 0x62000000 - l1_free->va;
	l1_free->pa = nil;
	l1_free->free = nil;
	l1_free->mapped = nil;
	l1_free->next = nil;

	initialized = true;
}

	int
take_span(struct l1_span *l, struct span *fs, size_t len)
{
	struct span *s, **ts;

	if (len < fs->len) {
		/* Split span */

		s = pool_alloc(&span_pool);
		if (s == nil) {
			log(LOG_INFO, "out of spans");
			return ERR;
		}

		s->pa = fs->pa + len;
		s->va = fs->va + len;
		s->len = fs->len - len;
		s->next = fs->next;
		fs->next = s;
		s->holder = &fs->next;

		fs->len = len;
	}

	ts = fs->holder;
	*ts = fs->next;
	if (*ts != nil) {
		(*ts)->holder = ts;
	}

	fs->next = nil;

	for (ts = &l->mapped; *ts != nil; ts = &(*ts)->next) {
		if (fs->va < (*ts)->va) {
			break;
		}
	}

	fs->holder = ts;
	fs->next = *ts;
	*ts = fs;

	return OK;
}

	int	
take_free_addr(size_t len, 
		struct l1_span **taken_l1_span,
		struct span **taken_span)
{
	struct l1_span *l, *fl, **tl;
	struct span *s, *fs;
	size_t pa, tlen;
	int r;

	fs = nil;
	for (l = l1_mapped; l != nil; l = l->next) {
		for (s = l->free; s != nil; s = s->next) {
			if (len <= s->len && (fs == nil || s->len < fs->len)) {
				*taken_l1_span = l;
				fs = s;
			}
		}
	}

	if (fs != nil) {
		*taken_span = fs;
		return take_span(*taken_l1_span, fs, len);
	}

	/* Need new l1 span */
	fl = nil;
	for (l = l1_free; l != nil; l = l->next) {
		if (l->len > len && (fl == nil || l->len < fl->len)) {
			fl = l;
		}
	}

	if (fl == nil) {
		return ERR;
	}

	s = pool_alloc(&span_pool);
	if (s == nil) {
		return ERR;
	}

	tlen = (TABLE_ALIGN(len) >> TABLE_SHIFT) * TABLE_SIZE;
	pa = request_memory(tlen);
	if (pa == nil) {
		pool_free(&span_pool, s);
		return ERR;
	}

	r = addr_map_l2s(pa, fl->va, tlen);
	if (r != OK) {
		release_addr(pa, tlen);
		pool_free(&span_pool, s);
		return ERR;
	}

	if (fl->len > TABLE_ALIGN(len)) {
		l = pool_alloc(&l1_span_pool);
		if (l == nil) {
			/* TODO: unmap and free table */
			pool_free(&span_pool, s);
			return ERR;
		}

		l->pa = nil;
		l->va = fl->va + TABLE_ALIGN(len);
		l->len = fl->len - TABLE_ALIGN(len);
		l->mapped = nil;
		l->free = nil;
		l->next = fl->next;
		fl->next = l;
		l->holder = &fl->next;

		fl->len = TABLE_ALIGN(len);
	}

	fl->pa = pa;

	tl = fl->holder;
	*tl = fl->next;
	if (fl->next != nil) {
		fl->next->holder = tl;
	}

	fl->next = nil;

	for (tl = &l1_mapped; *tl != nil; tl = &(*tl)->next) {
		if (fl->va < (*tl)->va) {
			break;
		}
	}

	fl->holder = tl;
	fl->next = *tl;
	*tl = fl;

	s->pa = nil;
	s->va = fl->va;
	s->len = fl->len;
	s->next = nil;
	s->holder = &fl->free;
	fl->free = s;

	*taken_l1_span = fl;
	*taken_span = fl->free;

	return take_span(fl, fl->free, len);
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
		if (!r) exit_r(0xff00);
		checking = false;
	}

	if (r && pool_n_free(&span_pool) < 8) {
		log(LOG_INFO, "growing span pool");
		checking = true;
		r = grow_pool(&span_pool);
		log(LOG_INFO, "pool grown ? %i", r);
		if (!r) exit_r(0xff01);
		checking = false;
	}

	return r;
}

	void *
map_addr(size_t pa, size_t len, int flags)
{
	struct l1_span *l;
	struct span *s;
	int r;

	if (!initialized)
		addr_init();

	if (PAGE_ALIGN(pa) != pa) {
		return nil;
	}

	if (!check_pools()) {
		return nil;
	}

	len = PAGE_ALIGN(len);

	if (take_free_addr(len, &l, &s) != OK) {
		return nil;
	}

	if ((r = addr_map(pa, s->va, len, flags)) != OK) {
		/* TODO: put span into free */
		log(LOG_INFO, "addr map failed %i for 0x%x -> 0x%x 0x%x", r, pa, s->va, len);
		return nil;
	}

	return (void *) s->va;
}

	int
unmap_addr(void *addr, size_t len)
{
	size_t va = (size_t) addr;
	struct span *s, **f;
	struct l1_span *l;
	
	if (addr_unmap(va, len) != OK) {
		exit_r(0x123456);
		return ERR;
	}

	if (!initialized)
		return ERR;

	for (l = l1_mapped; l != nil; l = l->next) {
		if (l->va <= va && va + len <= l->va + l->len) {
			break;
		}
	}

	if (l == nil) {
		log(LOG_INFO, "error unmapping, l1 not mapped");

		exit_r(0x1236);
		return ERR;
	}

	for (s = l->mapped; s != nil; s = s->next) {
		if (s->va <= va && va + len <= s->va + s->len) {
			break;
		}
	}

	if (s == nil) {
		log(LOG_INFO, "error unmapping, pages not mapped");
		exit_r(0x1235);
		return ERR;
	}

	if (s->va != va || s->len != len) {
		log(LOG_INFO, "error unmapping, cannot unmap across mappings");
		/* TODO: support unmapping parts of mapped space */
		exit_r(0x1234);
		return ERR;
	}

	/* Remove from mapped */
	*s->holder = s->next;
	if (s->next) s->next->holder = s->holder;

	for (f = &l->free; *f != nil; f = &(*f)->next) {
		if (s->va < (*f)->va) {
			break;
		}
	}

	/* Add to free */
	s->next = *f;
	if (s->next) s->next->holder = &s->next;

	*f = s;
	s->holder = f;

	/* Unmap */
	s->pa = nil;
	return OK;
}

