#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <stdarg.h>
#include <string.h>

#include "proc0.h"

static struct pool *pools = nil;

	static struct pool_frame *
pool_frame_init(struct pool *s, void *a, size_t pa, size_t len)
{
	struct pool_frame *f;

	memset(a, 0, len);

	f = a;

	f->next = nil;
	f->pa = pa;
	f->len = len;
	f->nobj = 
		(8 * len - 8 * sizeof(struct pool_frame)) / 
		(1 + 8 * s->obj_size);

	return f;
}

	struct pool *
pool_new_with_frame(size_t obj_size, 
		void *frame, size_t len)
{
	struct pool *s;

	if (pools == nil) {
		raise();
	}

	s = pools;
	pools = s->next;

	s->next = nil;
	s->head = nil;
	s->obj_size = obj_size;

	if (frame != nil) {
		s->head = pool_frame_init(s, frame, 0, len);
	}
	
	return s;
}

	struct pool *
pool_new(size_t obj_size)
{
	struct pool *n, *s;
	size_t pa;

	if (pools == nil) {
		pa = get_mem(0x1000, 0x1000);
		if (pa == nil) {
			return nil;
		}

		n = map_free(pa, 0x1000, MAP_MEM|MAP_RW);
		if (n == nil) {
			free_mem(pa, 0x1000);
			return nil;
		}

		for (s = n; (size_t) (s + 1) <= (size_t) n + 0x1000; s++) {
			s->next = pools;
			pools = s;
		}
	}

	return pool_new_with_frame(obj_size, nil, 0);
}

	void
pool_destroy(struct pool *s)
{

}

	static size_t
pool_alloc_from_frame(struct pool *s, struct pool_frame *f)
{
	size_t *u;
	int i;

	for (i = 0; i < f->nobj; i++) {
		u = &f->use[i / sizeof(size_t)];
		if (!(*u & (1 << (i % sizeof(size_t))))) {
			*u |=	(1 << (i % sizeof(size_t)));
			return (size_t) &f->use[f->nobj] + s->obj_size * i;
		}
	}	

	return nil;
}

	void *
pool_alloc(struct pool *s)
{
	struct pool_frame **f;
	size_t va, pa, len;

	for (f = &s->head; *f != nil; f = &(*f)->next) {
		va = pool_alloc_from_frame(s, *f);
		if (va != nil) {
			return (void *) va;
		}
	}

	len = 0x1000;

	pa = get_mem(len, 0x1000);
	if (pa ==  nil) {
		return nil;
	}

	*f = map_free(pa, len, MAP_MEM|MAP_RW);
	if (*f == nil) {
		free_mem(pa, len);
		return nil;
	}

	pool_frame_init(s, *f, pa, len);

	return (void *) pool_alloc_from_frame(s, *f);
}

	void
pool_free(struct pool *s, void *p)
{

}
	void
init_pools(void)
{
	static struct pool starting_pools[32] = { 0 };
	int i;

	for (i = 0; i < LEN(starting_pools); i++) {
		starting_pools[i].next = pools;
		pools = &starting_pools[i];
	}
}

