#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <stdarg.h>
#include <string.h>

#include "proc0.h"
#include "../dev.h"

struct mem_container {
	size_t pa, len;
	struct mem_container *next;
};

uint8_t mem_pool_init_frame[0x1000]
__attribute__((__aligned__(0x1000))) = { 0 };

uint8_t l3_pool_init_frame[0x1000]
__attribute__((__aligned__(0x1000))) = { 0 };

struct pool *mem_pool = nil;
struct mem_container *mem = nil;

struct pool init_pools[32] = { 0 };
struct pool *pools = nil;

struct pool *l1_pool = nil;
struct pool *l2_pool = nil;
struct pool *l3_pool = nil;

struct l1 proc0_l1 = { 0 };
struct l2 proc0_l2 = { 0 };

	static struct mem_container *
mem_container_split(struct mem_container *f, size_t off)
{
	struct mem_container *n;

	n = pool_alloc(mem_pool);
	if (n == nil) {
		return nil;
	}

	n->pa = f->pa + off;
	n->len = f->len - off;

	n->next = nil;

	f->len = off;

	return n;
}

	static struct mem_container *
get_free_mem(size_t len, size_t align)
{
	struct mem_container *f, *n;
	size_t a;

	for (f = mem; f != nil; f = f->next) {
		a = (f->pa + align - 1) & ~(align - 1);

		if (len < a - f->pa + f->len) {
			if (f->pa < a) {
				f = mem_container_split(f, a - f->pa);
				if (f == nil) 
					return nil;
			}

			n = mem_container_split(f, len);

			n->next = mem;
			mem = n;

			return f;
		}
	}

	return nil; 
}

	size_t
get_mem(size_t l, size_t align)
{
	struct mem_container *m;
	size_t pa;

	m = get_free_mem(l, align);
	if (m == nil) {
		return 0;
	}

	pa = m->pa;

	pool_free(mem_pool, m);

	return pa;
}

	void
free_mem(size_t a, size_t l)
{
	/* TODO */
}

static struct pool_frame *
pool_frame_init(struct pool *s, void *a, size_t pa, size_t len)
{
	struct pool_frame *f;

	memset(a, 0, len);

	f = a;
	/* TODO: this is probably not the best */

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

	struct l1 *
l1_create_from(size_t pa, void *addr, size_t len)
{
	struct l1 *l1;

	l1 = pool_alloc(l1_pool);
	if (l1 == nil) {
		return nil;
	}

	l1->pa = pa;
	l1->len = 0x4000;
	l1->addr = addr;
	l1->head = nil;

	l1->n_addr = 0x30000;

	return l1;

}

	struct l1 *
l1_create(void)
{
	struct l1 *l1;
	size_t s, l;
	void *addr;
	size_t pa;

	pa = get_mem(0x4000, 0x4000);
	if (pa == nil) {
		return nil;
	}

	addr = map_free(pa, 0x4000, MAP_DEV|MAP_RW);
	if (addr == nil) {
		free_mem(pa, 0x4000);
		return nil;
	}

	memset(addr, 0, 0x4000);

	s = info->kernel_start >> 20;
	l = (info->kernel_len >> 20) + 1;
	memcpy(&((uint32_t *) addr)[s], &info->l1_va[s], 
			l * sizeof(uint32_t));

	l1 = l1_create_from(pa, addr, 0x4000);
	if (l1 == nil) {
		unmap(addr, 0x4000);
		free_mem(pa, 0x4000);
		return nil;
	}

	return l1;
}

	void
l1_free(struct l1 *l)
{
	unmap(l->addr, l->len);
	free_mem(l->pa, l->len);
	pool_free(l1_pool, l);
}

	struct l2 *
l2_create_table_from(size_t len, size_t va, size_t pa, void *addr)
{
	struct l2 *n;

	n = pool_alloc(l2_pool);
	if (n == nil) {
		return nil;
	}

	n->pa = pa;
	n->va = va;
	n->len = len;
	n->next = nil;

	n->head = nil;
	n->addr = addr;

	return n;
}

	struct l2 *
l2_create_table(size_t len, size_t va)
{
	struct l2 *n;
	void *addr;
	size_t pa;

	pa = get_mem(len, 0x1000);
	if (pa == nil) {
		return nil;
	}

	addr = map_free(pa, len, MAP_DEV|MAP_RW);
	if (addr == nil) {
		free_mem(pa, len);
		return nil;
	}

	memset(addr, 0, len);

	n = l2_create_table_from(len, va, pa, addr);
	if (n == nil) {
		unmap(addr, len);
		free_mem(pa, len);
		return nil;
	}

	return n;
}

	void
l2_free(struct l2 *l)
{
	unmap(l->addr, l->len);
	free_mem(l->pa, l->len);
	pool_free(l2_pool, l);
}

	int
l1_insert_l2(struct l1 *l1, struct l2 *l2)
{
	struct l2 **l;

	for (l = &l1->head; *l != nil; l = &(*l)->next) {
		if (l2->va < (*l)->va) {
			if (l2->va + (l2->len << 10) < (*l)->va) {
				break;
			} else {
				return ERR;
			}

		} else if ((*l)->va + ((*l)->len << 10) < l2->va) {
			continue;

		} else if ((*l)->va <= l2->va 
				&& l2->va < (*l)->va + ((*l)->len << 10)) {
			return ERR;
		}
	}

	l2->next = *l;
	*l = l2;

	map_l2(l1->addr, l2->pa, l2->va, l2->len);

	return OK;
}

	struct l2 *
l1_get_l2(struct l1 *l1, size_t va)
{
	struct l2 *l;

	for (l = l1->head; l != nil; l = l->next) {
		if (l->va <= va && va < l->va + (l->len << 10)) {
			return l;
		}
	}

	return nil;
}

	int
l2_insert_l3(struct l2 *l2, struct l3 *l3)
{
	struct l3 **l;
	bool cache;
	int ap;

	for (l = &l2->head; *l != nil; l = &(*l)->next) {
		if (l3->va < (*l)->va) {
			if (l3->va + l3->len < (*l)->va) {
				break;
			} else {
				return ERR;
			}

		} else if ((*l)->va + (*l)->len < l3->va) {
			continue;

		} else if ((*l)->va <= l3->va 
				&& l3->va < (*l)->va + (*l)->len) {
			return ERR;
		}
	}

	l3->next = *l;
	*l = l3;

	if (l3->flags & MAP_RW) {
		ap = AP_RW_RW;
	} else {
		ap = AP_RW_RO;
	}

	if ((l3->flags & MAP_TYPE_MASK) == MAP_MEM) {
		cache = true;
	} else if ((l3->flags & MAP_TYPE_MASK) == MAP_DEV) {
		cache = false;
	} else {
		return ERR;
	}

	ap = AP_RW_RW;
	cache = false;
	return map_pages(l2->addr, l3->pa, l3->va, l3->len, ap, cache);
}

	size_t
l1_random_va(struct l1 *l1, size_t len)
{
	size_t va;

	va = l1->n_addr;
	l1->n_addr = SECTION_ALIGN(l1->n_addr + len);

	return va;
}

	struct l3 *
l3_create(size_t pa, size_t va, size_t len, int flags)
{
	struct l3 *l3;

	l3 = pool_alloc(l3_pool);
	if (l3 == nil) {
		return nil;
	}

	l3->pa = pa;
	l3->va = va;
	l3->len = len;
	l3->flags = flags;

	return l3;
}

	void
l3_free(struct l3 *l3)
{
	pool_free(l3_pool, l3);
}

/* TODO: Do these properly */

	
static size_t a = 0x30000;

	void *
map_free(size_t pa, size_t len, int flags)
{
	size_t va;
	va = a;
	a += len;

	struct l3 *l3;

	l3 = l3_create(pa, va, len, flags);
	l2_insert_l3(&proc0_l2, l3);

	return (void *) va;
}

	void
unmap(void *va, size_t len)
{

}

	void
init_mem(void)
{
	struct mem_container *m;
	size_t l1_pa, l2_pa;
	int i;

	for (i = 0; i < LEN(init_pools); i++) {
		init_pools[i].next = pools;
		pools = &init_pools[i];
	}

	mem_pool = 
		pool_new_with_frame(sizeof(struct mem_container), 
				mem_pool_init_frame, sizeof(mem_pool_init_frame));

	if (mem_pool == nil) {
		raise();
	}

	for (i = 0; i < ndevices; i++) {
		if (!strncmp(devices[i].compatable, "mem", 
					sizeof(devices[i].compatable)))
			continue;

		m = pool_alloc(mem_pool);
		if (m == nil) {
			raise();
		}

		m->pa  = devices[i].reg;
		m->len = devices[i].len;
		m->next = mem;
		mem = m;
	}

	if (mem == nil) {
		raise();
	}

	l1_pool = pool_new(sizeof(struct l1));
	if (l1_pool == nil) {
		raise();
	}

	l2_pool = pool_new(sizeof(struct l2));
	if (l2_pool == nil) {
		raise();
	}

	l3_pool = pool_new_with_frame(sizeof(struct l3), 
				l3_pool_init_frame, sizeof(l3_pool_init_frame));
	if (l3_pool == nil) {
		raise();
	}

	l1_pa = info->l2_va[L2X((uint32_t) info->l1_va)] & ~0x3;
	l2_pa = info->l2_va[L2X((uint32_t) info->l2_va)] & ~0xfff;

	proc0_l1.pa = l1_pa;
	proc0_l1.len = 0x4000;
	proc0_l1.addr = info->l1_va;
	proc0_l1.head = nil;
	proc0_l1.n_addr = 0x30000;

	procs[0].pid = 0;
	procs[0].l1 = &proc0_l1;

	proc0_l2.pa = l2_pa;
	proc0_l2.va = (size_t) info->l2_va;
	proc0_l2.len = 0x1000;
	proc0_l2.next = nil;
	proc0_l2.head = nil;
	proc0_l2.addr = info->l2_va;

	if (l1_insert_l2(&proc0_l1, &proc0_l2) != OK) {
		raise();
	}

	/* TODO: add mappings for stack, code, info. */
}

