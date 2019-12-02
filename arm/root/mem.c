#include "head.h"
#include <arm/mmu.h>
#include <log.h>

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
				log(LOG_INFO, "range 0x%x 0x%x is too small",
						(*f)->start, (*f)->len, start, len);
				return nil;
			}

			if ((*f)->start < start) {
				m = addr_range_split(*f, start - (*f)->start);
				if (m == nil) {
					log(LOG_INFO, "split failed");
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

	log(LOG_INFO, "0x%x 0x%x not found", start, len);
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
request_memory(size_t len, size_t align)
{
	struct addr_range *m;
	size_t pa;

	m = addr_range_get_any(&ram_free, len, align);
	if (m == nil) {
		return nil;
	}

	pa = m->start;

	pool_free(&addr_pool, m);

	log(LOG_INFO, "giving away 0x%x 0x%x", pa, len);

	int cid;

	cid = kobj_alloc(OBJ_frame, 1);
	if (cid < 0) {
		log(LOG_WARNING, "frame alloc failed");
		return ERR;
	}

	if (frame_setup(cid, FRAME_MEM, m->start, m->len) != OK) {
		log(LOG_WARNING, "frame setup failed");
		return ERR;
	}

	return cid;
}

	int
release_memory(int cid)
{
	/* TODO */
	return ERR;
}

#if 0
/* TODO: should make this use the code in proc.c
	 and manage itself like the other procs. */

	int
addr_map_l2s(size_t pa, size_t va, size_t tlen)
{
	uint32_t o, *addr;

	addr = map_addr(pa, tlen, MAP_RW|MAP_DEV);
	if (addr == nil) {
		return PROC0_ERR_INTERNAL;
	}

	memset(addr, 0, tlen);

	for (o = 0; (o << 10) < tlen; o++) {
		if (root_l1.mmu.addr[L1X(va) + o] != L1_FAULT) {
			return PROC0_ERR_ADDR_DENIED;
		}

		root_l1.mmu.addr[L1X(va) + o]
			= (pa + (o << 10)) | L1_COARSE;

		root_l1.va[L1X(va) + o]
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

	if (info->kernel_va <= va + len) {
		log(LOG_WARNING, "map 0x%x + 0x%x would be into kernel memory 0x%x",
				va, len, info->kernel_va);
		return PROC0_ERR_ADDR_DENIED;
	}

	if (flags & MAP_RW) {
		ap = AP_RW_RW;
	} else {
		ap = AP_RW_RO;
	}

	if ((flags & MAP_TYPE_MASK) == MAP_MEM) {
		cache = true;
	} else if ((flags & MAP_TYPE_MASK) == MAP_DEV) {
		cache = false;
	} else {
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

		l2 = root_l1.va[L1X(va + o)];
		if (l2 == nil) {
			log(LOG_INFO, "l1 nil");
			return PROC0_ERR_TABLE;
		}

		if (l2[L2X(va + o)] != L2_FAULT) {
			log(LOG_INFO, "l2 already mapped 0x%x", l2[L2X(va + o)]);
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
		log(LOG_INFO, "unmap page 0x%x", va + o);

		l2 = root_l1.va[L1X(va + o)];
		if (l2 == nil) {
			log(LOG_INFO, "l1 nil");
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
	size_t kernel_index = L1X(info->kernel_va);

	log(LOG_INFO, "init l1 at 0x%x kva at 0x%x", l1, info->kernel_va);

	log(LOG_INFO, "memset 0x%x bytes", kernel_index * sizeof(uint32_t));
	memset(l1, 0, kernel_index * sizeof(uint32_t));

	log(LOG_INFO, "init l1 at 0x%x kva at 0x%x", l1, info->kernel_va);

	log(LOG_INFO, "copy 0x%x bytes", 0x4000 - kernel_index * sizeof(uint32_t));

	memcpy(&l1[kernel_index],
			&root_l1.mmu.addr[kernel_index],
			0x4000 - kernel_index * sizeof(uint32_t));

	log(LOG_INFO, "init l1 at 0x%x kva at 0x%x", l1, info->kernel_va);
}

#endif

	void
add_ram(size_t start, size_t len)
{
	struct addr_range *m;

	m = pool_alloc(&addr_pool);
	if (m == nil) {
		log(LOG_FATAL, "pool alloc failed for adding ram");
		exit(1);
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
		exit(1);
	}

	if (pool_load(&addr_pool, addr_pool_initial, sizeof(addr_pool_initial)) != OK) {
		exit(2);
	}

	board_init_ram();

	if (ram_free == nil) {
		exit(3);
	}

	/* Remove stuff from ram range */

	log(LOG_INFO, "remove boot range 0x%x 0x%x", info->boot_pa, info->boot_len);

	if ((m = addr_range_get(&ram_free, info->boot_pa, info->boot_len)) == nil) {
		exit(4);
	}

	pool_free(&addr_pool, m);

	log(LOG_INFO, "remove kernel range 0x%x 0x%x", info->kernel_pa, info->kernel_len);

	if ((m = addr_range_get(&ram_free, info->kernel_pa, info->kernel_len)) == nil) {
		exit(5);
	}

	pool_free(&addr_pool, m);

	log(LOG_INFO, "remove root range 0x%x 0x%x", info->root_pa, info->root_len);

	if ((m = addr_range_get(&ram_free, info->root_pa, info->root_len)) == nil) {
		exit(6);
	}

	pool_free(&addr_pool, m);

	log(LOG_INFO, "remove bundle range 0x%x 0x%x", info->bundle_pa, info->bundle_len);

	if ((m = addr_range_get(&ram_free, info->bundle_pa, info->bundle_len)) == nil) {
		exit(7);
	}

	pool_free(&addr_pool, m);
}

