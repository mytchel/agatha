#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>

#include "proc0.h"
#include "../bundle.h"

struct l3 {
	struct l3 *next;

	size_t pa, va, len;
	int flags;
};

struct l2 {
	struct l2 *next;

	size_t pa, va, len;
	struct l3 *head;
	void *addr;
};

struct l1 {
	size_t pa, len;
	void *addr;
	struct l2 *head;

	/* Temporary unsafe way of easily getting a free address. */
	size_t n_addr;
};

struct proc {
	int pid;
	struct l1 *l1;
};

static struct pool *l1_pool;
static struct pool *l2_pool;
static struct pool *l3_pool;

static struct proc procs[MAX_PROCS] = { 0 };

	static struct l1 *
proc_create_l1(void)
{
	struct l1 *l1;
	size_t s, l;
	void *addr;
	size_t pa;

	pa = get_mem(0x4000, 0x4000);
	if (pa == nil) {
		return nil;
	}

	addr = map_free(pa, 0x4000, AP_RW_RW, false);
	if (addr == nil) {
		free_mem(pa, 0x4000);
		return nil;
	}

	l1 = pool_alloc(l1_pool);
	if (l1 == nil) {
		unmap(addr, 0x4000);
		free_mem(pa, 0x4000);
		return nil;
	}

	l1->pa = pa;
	l1->len = 0x4000;
	l1->addr = addr;
	l1->head = nil;

	l1->n_addr = 0x100000;

	memset(l1->addr, 0, 0x4000);

	s = info->kernel_start >> 20;
	l = 0x1000 - s;/*(info->kernel_len >> 20) + 1;*/
	memcpy(&((uint32_t *) l1->addr)[s], &info->l1_va[s], 
			l * sizeof(uint32_t));

	return l1;
}

	static void
proc_free_l1(struct l1 *l)
{

}

	static struct l2 *
proc_create_l2_table(size_t len, size_t va)
{
	struct l2 *n;
	void *addr;
	size_t pa;

	pa = get_mem(len, 0x1000);
	if (pa == nil) {
		return nil;
	}

	addr = map_free(pa, len, AP_RW_RW, true);
	if (addr == nil) {
		free_mem(pa, len);
		return nil;
	}

	n = pool_alloc(l2_pool);
	if (n == nil) {
		unmap(addr, len);
		free_mem(pa, len);
		return nil;
	}

	n->pa = pa;
	n->va = va;
	n->len = len;
	n->next = nil;

	n->head = nil;
	n->addr = addr;

	memset(addr, 0, len);

	return n;
}

	static void
proc_free_l2(struct l2 *l)
{

}

	static int
proc_insert_l1_l2(struct l1 *l1, struct l2 *l2)
{
	struct l2 **l;
	size_t i;

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

	static struct l2 *
proc_get_l2(struct l1 *l1, size_t va)
{
	struct l2 *l;

	for (l = l1->head; l != nil; l = l->next) {
		if (l->va <= va && va < l->va + (l->len << 10)) {
			return l;
		}
	}

	return nil;
}

	static int
proc_insert_l2_l3(struct l2 *l2, struct l3 *l3)
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

	ap = AP_RW_RW;
	cache = true;

	return map_pages(l2->addr, l3->pa, l3->va, l3->len, ap, cache);
}

	static size_t
proc_random_va(struct l1 *l1, size_t len)
{
	size_t va;

	va = l1->n_addr;
	l1->n_addr = SECTION_ALIGN(l1->n_addr + len);

	return va;
}

	size_t
proc_map(size_t pid, 
		size_t pa, size_t va, 
		size_t len, int flags)
{
	struct l1 *l1;
	struct l2 *l2;
	struct l3 *l3;

	/* For now don't let mappings cross boundaries. */
	if (L1X(va) != L1X(va + len - 1)) {
		return nil;
	}

	l1 = procs[pid].l1;
	if (l1	== nil) {
		return nil;
	}

	if (va == nil) {
		va = proc_random_va(l1, len);
	}

	if (info->kernel_start <= va + len) {
		return nil;
	}

	l2 = proc_get_l2(l1, va);
	if (l2 == nil) {
		l2 = proc_create_l2_table(0x1000, L1X(va));
		if (l2 == nil) {
			return nil;
		}

		if (proc_insert_l1_l2(l1, l2) != OK) {
			proc_free_l2(l2);
			return nil;
		}
	}

	l3 = pool_alloc(l3_pool);
	if (l3 == nil) {
		return nil;
	}

	l3->pa = pa;
	l3->va = va;
	l3->len = len;
	l3->flags = flags;

	if (proc_insert_l2_l3(l2, l3) != OK) {
		pool_free(l3_pool, l3);
		return nil;
	}

	return l3->va;
}

	static bool
init_bundled_proc(char *name,
		size_t start, size_t len)
{
	uint32_t m[MESSAGE_LEN/sizeof(uint32_t)] = { 0 };
	size_t stack_p, slen;
	void *stack_v;
	struct l1 *l1;
	int pid;

	l1 = proc_create_l1();
	if (l1 == nil) {
		return false;
	}

	pid = proc_new();
	if (pid < 0) {
		proc_free_l1(l1);
		return false;
	}

	procs[pid].pid = pid;
	procs[pid].l1 = l1;

	if (va_table(pid, l1->pa) != OK) {
		procs[pid].pid = 0;
		proc_free_l1(l1);
		return false;
	}

	slen = 0x1000;

	stack_p = get_mem(slen, 0x1000); 
	if (stack_p == nil) {
		return false;
	}

	stack_v = map_free(stack_p, slen, AP_RW_RW, true);
	memset(stack_v, 0, slen);
	unmap(stack_v, slen);

	if (proc_map(pid, stack_p, USER_ADDR - slen, slen, 0) != USER_ADDR - slen) {
		return false;
	}

	if (proc_map(pid, start, USER_ADDR, len, 0) != USER_ADDR) {
		return false;
	}

	m[0] = USER_ADDR;
	m[1] = USER_ADDR;

	return send(pid, (uint8_t *) m) == OK;
}

	void
start_bundled_procs(void)
{
	size_t off;
	int i;

	off = info->bundle_start;

	for (i = 0; i < nbundled_procs; i++) {
		if (!init_bundled_proc(bundled_procs[i].name, 
					off, bundled_procs[i].len)) {
			raise();
		}

		off += bundled_procs[i].len;
	}
}

	void
init_procs(void)
{
	l1_pool = pool_new(sizeof(struct l1));
	if (l1_pool == nil) {
		raise();
	}

	l2_pool = pool_new(sizeof(struct l2));
	if (l2_pool == nil) {
		raise();
	}

	l3_pool = pool_new(sizeof(struct l3));
	if (l3_pool == nil) {
		raise();
	}

	map_sections(info->l1_va, 0x63000000, 0x63000000, 0x1000000, AP_RW_RW, false);

	start_bundled_procs();
}

