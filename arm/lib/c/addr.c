#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <arm/mmu.h>
#include <arm/mmu.h>
#include <log.h>

struct span {
	uint32_t pa, va, len;
	struct span **holder, *next;
};

struct l1_span {
	uint32_t pa, va, len;
	
	struct span *free, *mapped;

	struct l1_span **holder, *next;
};

struct span_pool {
	struct span_pool *next;
	size_t pa, len;
	size_t nspans;
	struct span spans[];
};

struct l1_span_pool {
	struct l1_span_pool *next;
	size_t nspans;
	size_t nfree_spans;
	struct l1_span spans[];
};

static struct l1_span l1_span_initial[4];
static struct span span_initial[8];

static size_t n_spans_free;
static size_t n_l1_spans_free;
static struct span *spans_free;
static struct l1_span *l1_spans_free;

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

	struct span *
span_new(void)
{
	struct span *s;

	if (spans_free == nil) {
		return nil;
	}

	s = spans_free;
	spans_free = s->next;
	n_spans_free--;

	return s;
}

void
span_free(struct span *s)
{
	s->next = spans_free;
	spans_free = s;
	n_spans_free++;
}

struct l1_span *
l1_span_new(void)
{
	struct l1_span *s;

	if (l1_spans_free == nil) {
		return nil;
	}

	s = l1_spans_free;
	l1_spans_free = s->next;
	n_l1_spans_free--;

	return s;
}

void
l1_span_free(struct l1_span *l)
{
	l->next = l1_spans_free;
	l1_spans_free = l;
	n_l1_spans_free++;
}

void
addr_init(void)
{
	struct span *m, *f, *s;
	struct l1_span *l;
	size_t i;

	for (i = 0; i < sizeof(span_initial)/sizeof(span_initial[0]); i++) {
		s = &span_initial[i];
		s->next = spans_free;
		spans_free = s;
		n_spans_free++;
	}

	for (i = 0; i < sizeof(l1_span_initial)/sizeof(l1_span_initial[0]); i++) {
		l = &l1_span_initial[i];
		l->next = l1_spans_free;
		l1_spans_free = l;
		n_l1_spans_free++;
	}

	l1_mapped = l1_span_new();
	l1_free = l1_span_new();

	/* TODO: ignores space before text start (this 
		 is currently where proc0 maps the stack
		 and the l2 table which we could use to
		 find more of this information) */

	l1_mapped->holder = &l1_mapped;
	l1_mapped->next = nil;
	l1_mapped->pa = nil;
	l1_mapped->va = TABLE_ALIGN_DN(&_text_start);
	l1_mapped->len = TABLE_ALIGN(&_data_end) - l1_mapped->va;

	m = span_new();
	l1_mapped->mapped = m;
	m->holder = &l1_mapped->mapped;
	m->va = PAGE_ALIGN_DN(&_text_start);
	m->len = PAGE_ALIGN(&_data_end) - m->va;
	m->pa = nil;
	m->next = nil;

	f = span_new();
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
	
	if (fs->len > len) {
		/* Split span */

		s = span_new();
		if (s == nil) {
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
			if (s->len > len && (fs == nil || s->len < fs->len)) {
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

	s = span_new();
	if (s == nil) {
		return ERR;
	}

	tlen = (TABLE_ALIGN(len) >> TABLE_SHIFT) * TABLE_SIZE;
	pa = request_memory(tlen);
	if (pa == nil) {
		span_free(s);
		return ERR;
	}

	r = addr_map_l2s(pa, fl->va, tlen);
	if (r != OK) {
		release_addr(pa, tlen);
		span_free(s);
		return ERR;
	}

	if (fl->len > TABLE_ALIGN(len)) {
		l = l1_span_new();
		if (l == nil) {
			/* TODO: unmap and free table */
			span_free(s);
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

	void *
map_addr(size_t pa, size_t len, int flags)
{
	struct l1_span *l;
	struct span *s;
	int r;

	if (!initialized)
		addr_init();

	log(LOG_INFO, "map 0x%x 0x%x", pa, len);

	if (PAGE_ALIGN(pa) != pa) return nil;

	len = PAGE_ALIGN(len);

	if (take_free_addr(len, &l, &s) != OK) {
		return nil;
	}

	if ((r = addr_map(pa, s->va, len, flags)) != OK) {
		/* TODO: put span into free */
		exit_r(r);
		return nil;
	}

	return (void *) s->va;
}

int
unmap_addr(void *addr, size_t len)
{
	addr_unmap((size_t) addr, len);
	return ERR;
}


