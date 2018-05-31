#include "head.h"
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
struct mem_container *ram_free = nil;
struct mem_container *dev_regs = nil;

/* TODO; make sure l2_pool and l3_pool never become
	 empty, otherwise proc0 can not map and so cannot
	 add to them. */
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

int
get_regs(size_t pa, size_t len)
{
	struct mem_container **f, *m, *n;

	for (f = &dev_regs; *f != nil; f = &(*f)->next) {
		if ((*f)->pa <= pa && pa < (*f)->pa + (*f)->len) {
			if ((*f)->pa + (*f)->len < pa + len) {
				return ERR;
			}

			if ((*f)->pa < pa) {
				m = mem_container_split(*f, pa - (*f)->pa);
				if (m == nil) {
					return ERR;
				}

			} else {
				m = *f;
				*f = (*f)->next;
			}

			n = mem_container_split(m, len);

			n->next = dev_regs;
			dev_regs = n;

			break;
		}	
	}

	if (m == nil) {
		return ERR;
	}

	pool_free(mem_pool, m);

	return OK;
}

	size_t
get_ram(size_t len, size_t align)
{
	struct mem_container **f, *m, *n;
	size_t a, pa;

	for (f = &ram_free; *f != nil; f = &(*f)->next) {
		a = ((*f)->pa + align - 1) & ~(align - 1);

		if (len < a - (*f)->pa + (*f)->len) {
			if ((*f)->pa < a) {
				m = mem_container_split(*f, a - (*f)->pa);
				if (m == nil) {
					return nil;
				}

			} else {
				m = *f;
				*f = (*f)->next;	
			}

			n = mem_container_split(m, len);

			n->next = ram_free;
			ram_free = n;

			break;
		}
	}

	if (m == nil) {
		return nil;
	}

	pa = m->pa;

	pool_free(mem_pool, m);

	return pa;
}

	void
free_mem(size_t pa, size_t len)
{
	/* TODO */
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

	return l1;

}

	struct l1 *
l1_create(void)
{
	struct l1 *l1;
	size_t s, l;
	void *addr;
	size_t pa;

	pa = get_ram(0x4000, 0x4000);
	if (pa == nil) {
		return nil;
	}

	addr = map_free(pa, 0x4000, MAP_SHARED|MAP_RW);
	if (addr == nil) {
		free_mem(pa, 0x4000);
		return nil;
	}

	memset(addr, 0, 0x4000);

	s = L1X(info->kernel_pa);
	l = 0x1000 - s;
	memcpy(&((uint32_t *) addr)[s], &info->proc0.l1_va[s], 
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

	pa = get_ram(len, 0x1000);
	if (pa == nil) {
		return nil;
	}

	addr = map_free(pa, len, MAP_SHARED|MAP_RW);
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
	} else if ((l3->flags & MAP_TYPE_MASK) == MAP_SHARED) {
		cache = false;
	} else {
		return ERR;
	}

	return map_pages(l2->addr, l3->pa, l3->va, l3->len, ap, cache);
}

	size_t
l1_free_va(struct l1 *l1, size_t len)
{
	struct l2 *l2;
	struct l3 *l3;
	size_t va;

	va = 0x1000;
	for (l2 = l1->head; l2 != nil; l2 = l2->next) {
		for (l3 = l2->head; l3 != nil; l3 = l3->next) {
			if (va + len < l3->va) {
				return va;
			}	else {
				if (L1X(va) != L1X(l3->va + l3->len)) {
					va = L1VA(L1X(l3->va + l3->len));
					break;
				}

				va = l3->va + l3->len;
				if (l3->next == nil && L1X(va) == L1X(va + len)) {
					return va;
				}
			}
		}
	}

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

	void *
map_free(size_t pa, size_t len, int flags)
{
	struct l2 *l2;
	struct l3 *l3;
	size_t va;

	va = l1_free_va(&proc0_l1, len);
	if (va == nil) {
		raise();
	}

	l2 = l1_get_l2(&proc0_l1, va);
	if (l2 == nil) {
		/* TODO: this should be handled */
		raise();		
	}

	l3 = l3_create(pa, va, len, flags);
	if (l3 == nil) {
		/* TODO: this should be handled */
		raise();
	}

	if (l2_insert_l3(l2, l3) != OK) {
		raise();
	}

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
	struct l3 *l3;
	int i;

	mem_pool = 
		pool_new_with_frame(sizeof(struct mem_container), 
				mem_pool_init_frame, sizeof(mem_pool_init_frame));

	if (mem_pool == nil) {
		raise();
	}

	for (i = 0; i < ndevices; i++) {
		m = pool_alloc(mem_pool);
		if (m == nil) {
			raise();
		}

		m->pa  = devices[i].reg;
		m->len = devices[i].len;

		if (strncmp(devices[i].compatable, "mem", 
					sizeof(devices[i].compatable))) {

			m->next = ram_free;
			ram_free = m;
		} else {
			m->next = dev_regs;
			dev_regs = m;
		}
	}

	if (ram_free == nil) {
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

	proc0_l1.pa = info->proc0.l1_pa;
	proc0_l1.len = info->proc0.l1_len;
	proc0_l1.addr = info->proc0.l1_va;
	proc0_l1.head = nil;

	procs[0].pid = 0;
	procs[0].l1 = &proc0_l1;

	proc0_l2.pa = info->proc0.l2_pa;
	proc0_l2.va = (size_t) info->proc0.l2_va;
	proc0_l2.len = info->proc0.l2_len;
	proc0_l2.next = nil;
	proc0_l2.head = nil;
	proc0_l2.addr = info->proc0.l2_va;

	if (l1_insert_l2(&proc0_l1, &proc0_l2) != OK) {
		raise();
	}

	l3 = l3_create(info->proc0.l1_pa,
			(size_t) info->proc0.l1_va,
			info->proc0.l1_len,
			MAP_SHARED|MAP_RW);

	if (l2_insert_l3(&proc0_l2, l3) != OK) {
		raise();
	}

	l3 = l3_create(info->proc0.l2_pa,
			(size_t) info->proc0.l2_va,
			info->proc0.l2_len,
			MAP_SHARED|MAP_RW);

	if (l2_insert_l3(&proc0_l2, l3) != OK) {
		raise();
	}

	l3 = l3_create(info->proc0.stack_pa,
			info->proc0.stack_va,
			info->proc0.stack_len,
			MAP_MEM|MAP_RW);

	if (l2_insert_l3(&proc0_l2, l3) != OK) {
		raise();
	}

	l3 = l3_create(info->proc0.prog_pa,
			info->proc0.prog_va,
			info->proc0.prog_len,
			MAP_MEM|MAP_RW);

	if (l2_insert_l3(&proc0_l2, l3) != OK) {
		raise();
	}

	l3 = l3_create(info->info_pa,
			info->proc0.info_va,
			info->info_len,
			MAP_MEM|MAP_RW);

	if (l2_insert_l3(&proc0_l2, l3) != OK) {
		raise();
	}	
}

