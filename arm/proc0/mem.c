#include "head.h"
#include "../dev.h"

struct l1 {
	struct {
		size_t pa, len;
		uint32_t *addr;
	} mmu;

	struct {
		size_t pa, len;
		uint32_t **addr;
	} va;
};

struct addr_range {
	size_t start, len;
	struct addr_range *next;
};

uint8_t addr_pool_init_frame[0x1000]
__attribute__((__aligned__(0x1000))) = { 0 };

struct pool *addr_pool = nil;

struct addr_range *ram_free = nil;
struct addr_range *dev_regs = nil;
struct addr_range *free_space = nil;

extern uint32_t *_data_end;

struct l1 proc0_l1;

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

	n = pool_alloc(addr_pool);
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

	int
get_regs(size_t pa, size_t len)
{
	struct addr_range *m;

	m = addr_range_get(&dev_regs, pa, len);
	if (m == nil) {
		m = addr_range_get(&ram_free, pa, len);
		if (m == nil) {
			return ERR;
		}
	}

	pool_free(addr_pool, m);

	return OK;
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

	pool_free(addr_pool, m);

	return pa;
}

	void
release_addr(size_t pa, size_t len)
{
	/* TODO */
}

static size_t next_l2_va, next_l2_pa;
static void *next_l2_addr;

static uint32_t *
put_next_l2(size_t *nva, size_t len)
{
	struct addr_range *m;
	uint32_t *tva;

	m = pool_alloc(addr_pool);
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

		pool_free(addr_pool, m);
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
init_mem(void)
{
	struct addr_range *m;
	int i;

	addr_pool = 
		pool_new_with_frame(sizeof(struct addr_range), 
				addr_pool_init_frame, 
				sizeof(addr_pool_init_frame));

	if (addr_pool == nil) {
		raise();
	}

	for (i = 0; i < ndevices; i++) {
		m = pool_alloc(addr_pool);
		if (m == nil) {
			raise();
		}

		m->start  = devices[i].reg;
		m->len = devices[i].len;

		if (strncmp(devices[i].compatable, "mem", 
					sizeof(devices[i].compatable))) {

			addr_range_insert(&ram_free, m);
		} else {
			addr_range_insert(&dev_regs, m);
		}
	}

	if (ram_free == nil) {
		raise();
	}

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
	m = pool_alloc(addr_pool);
	if (m == nil) {
		raise();
	}

	m->start = PAGE_ALIGN(&_data_end) + 0x4000 + 0x1000;
	m->len = SECTION_SIZE - (0x4000 + 0x1000);
	addr_range_insert(&free_space, m);
}

