#include "head.h"
#include <arm/mmu.h>
#include <log.h>
#include "../dev.h"

static struct {
	struct {
		size_t pa, len;
		uint32_t *addr;
	} mmu;

	uint32_t *va[0x1000];
} proc0_l1;

struct addr_range {
	size_t start, len;
	struct addr_range *next;
};

static uint8_t addr_pool_initial[sizeof(struct pool_frame)
	+ (sizeof(struct addr_range) + sizeof(struct pool_obj)) * 2048];

static struct pool addr_pool;

static struct addr_range *ram_free = nil;

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

	size_t
request_memory(size_t len)
{
	return get_ram(len, 0x1000);
}

	int
release_addr(size_t pa, size_t len)
{
	/* TODO */
	return ERR;
}

/* TODO: should make this use the code in proc.c
	 and manage itself like the other procs. */

	int
addr_map_l2s(size_t pa, size_t va, size_t tlen)
{
	uint32_t o, *addr;

	addr = map_addr(pa, tlen, MAP_RW|MAP_DEV);
	if (addr == nil) {
		exit_r(0xaa001);
		return PROC0_ERR_INTERNAL;
	}

	memset(addr, 0, tlen);

	for (o = 0; (o << 10) < tlen; o++) {
		if (proc0_l1.mmu.addr[L1X(va) + o] != L1_FAULT) {
			exit_r(0xaa002);
			return PROC0_ERR_ADDR_DENIED;
		}

		proc0_l1.mmu.addr[L1X(va) + o]
			= (pa + (o << 10)) | L1_COARSE;

		proc0_l1.va[L1X(va) + o]
			= (uint32_t *) (((uint32_t) addr) + (o << 10));
	}

	return OK;
}

int
addr_map(size_t pa, size_t va, size_t len, int flags)
{
	uint32_t tex, c, b, ap, o;
	uint32_t *l2;
	bool cache;

	if (flags & MAP_RW) {
		ap = AP_RW_RW;
	} else {
		ap = AP_RW_RO;
	}

	if ((flags & MAP_TYPE_MASK) == MAP_MEM) {
		cache = true;
	} else if ((flags & MAP_TYPE_MASK) == MAP_DEV) {
		cache = false;
	} else if ((flags & MAP_TYPE_MASK) == MAP_SHARED) {
		cache = false;
	} else {
		exit_r(0xaa003);
		return PROC0_ERR_FLAGS;
	}

	if (cache) {
		tex = 7;
		c = 1;
		b = 0;
	} else {
		tex = 0;
		c = 0;
		b = 1;
	}

	for (o = 0; o < len; o += PAGE_SIZE) {
		log(LOG_INFO, "map 0x%x -> 0x%x", va + o, pa + o);

		l2 = proc0_l1.va[L1X(va + o)];
		if (l2 == nil) {
			log(LOG_INFO, "l1 nil");
			exit_r(0xaa004);
			return PROC0_ERR_TABLE;
		}

		if (l2[L2X(va + o)] != L2_FAULT) {
			log(LOG_INFO, "l2 already mapped 0x%x", l2[L2X(va + o)]);
			exit_r(0xaa005);
			return PROC0_ERR_ADDR_DENIED;
		}

		l2[L2X(va + o)] = (pa + o)
			| L2_SMALL | tex << 6 | ap << 4 | c << 3 | b << 2;
	}

	return OK;
}

int
addr_unmap(size_t va, size_t len)
{
	uint32_t *l2;
	size_t o;

	for (o = 0; o < len; o += PAGE_SIZE) {
		log(LOG_INFO, "unmap 0x%x", va + o);

		l2 = proc0_l1.va[L1X(va + o)];
		if (l2 == nil) {
			log(LOG_INFO, "l1 nil");
			exit_r(0xaa004);
			return PROC0_ERR_TABLE;
		}

		l2[L2X(va + o)] = L2_FAULT;
	}

	yield();

	return OK;
}

void
proc_init_l1(uint32_t *l1)
{
	memset(l1, 0, 0x4000);

	memcpy(&l1[L1X(info->kernel_pa)],
			&proc0_l1.mmu.addr[L1X(info->kernel_pa)],
			(0x1000 - L1X(info->kernel_pa)) * sizeof(uint32_t));
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
	uint32_t o;

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

	memset(proc0_l1.va, 0, sizeof(proc0_l1.va));

	for (o = 0; (o << 10) < info->proc0.l2_len; o++) {
		proc0_l1.va[L1X(info->proc0.l2_va) + o]
			= (uint32_t *) (((uint32_t) info->proc0.l2_va) + (o << 10));
	}
}

