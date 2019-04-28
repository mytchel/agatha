#include "head.h"
#include <arm/mmu.h>
#include "../dev.h"

struct {
	struct {
		size_t pa, len;
		uint32_t *addr;
	} mmu;

	struct {
		size_t pa, len;
		uint32_t **addr;
	} va;
} proc0_l1;

struct addr_range {
	size_t start, len;
	struct addr_range *next;
};

static uint8_t addr_pool_initial[sizeof(struct pool_frame)
	+ (sizeof(struct addr_range) + sizeof(struct pool_obj)) * 1024];

static struct pool addr_pool;

static struct addr_range *ram_free = nil;
static struct addr_range *free_space = nil;

extern uint32_t *_data_end;

static void
addr_range_insert(struct addr_range **head, struct addr_range *n)
{
	n->next = *head;
	*head = n;
}

	static struct addr_range *
addr_range_split(struct addr_range *f, size_t off)
{
	struct addr_range *n;

	n = pool_alloc(&addr_pool);
	if (n == nil) {
		return nil;
	}

	n->start = f->start + off;
	n->len = f->len - off;

	n->next = nil;

	f->len = off;

	return n;
}

	static struct addr_range *
addr_range_get(struct addr_range **head, size_t start, size_t len)
{
	struct addr_range **f, *m, *n;

	for (f = head; *f != nil; f = &(*f)->next) {
		if ((*f)->start <= start && start < (*f)->start + (*f)->len) {
			if ((*f)->start + (*f)->len < start + len) {
				return nil;
			}

			if ((*f)->start < start) {
				m = addr_range_split(*f, start - (*f)->start);
				if (m == nil) {
					return nil;
				}

				m->next = (*f)->next;
				(*f)->next = m;
				f = &(*f)->next;
			}

			m = *f;
			if ((*f)->len > len) {
				n = addr_range_split((*f), len);

				n->next = (*f)->next;
				*f = n;
			} else {
				*f = (*f)->next;
			}

			return m;
		}	
	}

	return nil;
}
	
	static struct addr_range *
addr_range_get_any(struct addr_range **head, size_t len, size_t align)
{
	struct addr_range **f, *m, *n;
	size_t a;

	for (f = head; *f != nil; f = &(*f)->next) {
		a = ((*f)->start + align - 1) & ~(align - 1);

		if (len < a - (*f)->start + (*f)->len) {
			if ((*f)->start < a) {
				m = addr_range_split(*f, a - (*f)->start);
				if (m == nil) {
					return nil;
				}

				m->next = (*f)->next;
				(*f)->next = m;
				f = &(*f)->next;
			}

			m = *f;
			if ((*f)->len > len) {
				n = addr_range_split(*f, len);

				n->next = (*f)->next;
				*f = n;
			} else {
				*f = (*f)->next;
			}

			return m;
		}
	}

	return nil;
}

	size_t
get_ram(size_t len, size_t align)
{
	struct addr_range *m;
	size_t pa;

	m = addr_range_get_any(&ram_free, len, align);
	if (m == nil) {
		return nil;
	}

	pa = m->start;

	pool_free(&addr_pool, m);

	return pa;
}

	void
free_addr(size_t pa, size_t len)
{
	/* TODO */
}

/* TODO:
	 there are bugs here and i don't think we get any
	 more than one page of l2's 
 */

static size_t next_l2_va, next_l2_pa;
static void *next_l2_addr;

static uint32_t *
put_next_l2(size_t *nva, size_t len)
{
	struct addr_range *m;
	uint32_t *tva;

	m = pool_alloc(&addr_pool);
	if (m == nil) {
		raise();
	}

	/* Add remaining mappings from l2 as free */
	m->start = next_l2_va + 0x1000 + len;
	m->len = SECTION_SIZE - 0x1000 - len;
	addr_range_insert(&free_space, m);

	proc0_l1.mmu.addr[L1X(next_l2_va)] = next_l2_pa | L1_COARSE;
	proc0_l1.va.addr[L1X(next_l2_va)] = next_l2_addr;

	tva = next_l2_addr;
	*nva = next_l2_va + 0x1000;
	
	next_l2_pa = get_ram(0x1000, 0x1000);
	if (next_l2_pa == nil) {
		raise();
	}

	next_l2_addr = (void *) next_l2_va;
	next_l2_va += SECTION_SIZE;

	map_pages(tva, 
			next_l2_pa, 
			(size_t) next_l2_addr, 
			0x1000, AP_RW_RW, false);

	memset(next_l2_addr, 0, 0x1000);

	return tva;
}

	void *
map_free(size_t pa, size_t len, int ap, bool cache)
{
	struct addr_range *m;
	uint32_t *l2;
	size_t va;

	if (len > 0xff000) {
		return nil;
	}

	m = addr_range_get_any(&free_space, len, 0x1000);
	if (m != nil) {
		va = m->start;
		l2 = proc0_l1.va.addr[L1X(va)];

		pool_free(&addr_pool, m);
	} else {
		l2 = put_next_l2(&va, len);
	}

	if (l2 == nil) {
		raise();
	}

	map_pages(l2, pa, va, len, ap, cache);

	return (void *) va;
}

	void
unmap(void *addr, size_t len)
{

}

void
add_ram(size_t start, size_t len)
{
	struct addr_range *m;

	m = pool_alloc(&addr_pool);
	if (m == nil) {
		raise();
	}

	m->start = start;
	m->len = len;

	addr_range_insert(&ram_free, m);
}

	void
init_mem(void)
{
	struct addr_range *m;

	if (pool_init(&addr_pool, sizeof(struct addr_range)) != OK) {
		raise();
	}

	if (pool_load(&addr_pool, addr_pool_initial, sizeof(addr_pool_initial)) != OK) {
		raise();
	}
	
	board_init_ram();

	if (ram_free == nil) {
		raise();
	}

	/* Remove kernel from ram free */
	m = addr_range_get(&ram_free, info->kernel_pa, info->kernel_len);
	if (m == nil) {
		raise();
	}

	pool_free(&addr_pool, m);

	proc0_l1.mmu.pa = info->proc0.l1_pa;
	proc0_l1.mmu.len = info->proc0.l1_len;
	proc0_l1.mmu.addr = info->proc0.l1_va;

	proc0_l1.va.len = 0x4000;
	proc0_l1.va.addr = (uint32_t **) PAGE_ALIGN(&_data_end);
	proc0_l1.va.pa = get_ram(0x4000, 0x1000);
	if (proc0_l1.va.pa == nil) {
		raise();
	}

	map_pages(info->proc0.l2_va,
			proc0_l1.va.pa,
			(size_t) proc0_l1.va.addr,
			0x4000,
			AP_RW_RW, true);

	proc0_l1.va.addr[L1X(info->proc0.l2_va)] =
		info->proc0.l2_va;

	next_l2_pa = get_ram(0x1000, 0x1000);
	if (next_l2_pa == nil) {
		raise();
	}

	next_l2_addr = (void *) 
		(PAGE_ALIGN(&_data_end) + 0x4000);

	next_l2_va = SECTION_ALIGN(
			PAGE_ALIGN(&_data_end) 
			+ 0x4000 + 0x1000);

	map_pages(info->proc0.l2_va, 
			next_l2_pa, 
			(size_t) next_l2_addr, 
			0x1000, 
			AP_RW_RW, false);

	memset(next_l2_addr, 0, 0x1000);

	/* Add remaining mappings from init l2 as free */
	m = pool_alloc(&addr_pool);
	if (m == nil) {
		raise();
	}

	m->start = PAGE_ALIGN(&_data_end) + 0x4000 + 0x1000;
	m->len = SECTION_SIZE - (0x4000 + 0x1000);
	addr_range_insert(&free_space, m);
}

