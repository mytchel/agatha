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
create_frame(size_t pa, size_t len, int type)
{
	int cid;

	cid = kobj_alloc(OBJ_frame, 1);
	if (cid < 0) {
		log(LOG_WARNING, "frame alloc failed");
		exit(ERR);
	}

	if (frame_setup(cid, type, pa, len) != OK) {
		log(LOG_WARNING, "frame setup failed");
		exit(ERR);
	}

	return cid;
}

	int
request_memory(size_t len, size_t align)
{
	struct addr_range *m;

	m = addr_range_get_any(&ram_free, len, align);
	if (m == nil) {
		return nil;
	}

	int cid;

	cid = kobj_alloc(OBJ_frame, 1);
	if (cid < 0) {
		log(LOG_WARNING, "frame alloc failed");
		exit(ERR);
	}

	if (frame_setup(cid, FRAME_MEM, m->start, m->len) != OK) {
		log(LOG_WARNING, "frame setup failed");
		exit(ERR);
	}

	pool_free(&addr_pool, m);

	return cid;
}

	int
release_memory(int cid)
{
	/* TODO */
	return ERR;
}

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

