#include "head.h"
#include "../bundle.h"
#include <arm/mmu.h>

/* 

TODO: Most of this file needs improvements.
Frames should be objects that can contains a number
of address ranges and but can be treated as one piece? 

TODO: Frames should be able to be split.

TODO: L2 tables makes multiple proc0 mappings 
if they are mapped multiple times 

TODO: It is not very efficient and could be easily
improved.

TODO: L1 tables need to be made less
special so processes can map them 
in the same way they can map L2 tables.

 */

struct addr_frame {
	struct addr_frame *next;
	size_t pa, len;
	int table;
	int mapped;
};

struct proc {
	struct addr_frame *frames;

	struct {
		size_t pa, len;
		uint32_t *table;	
		uint32_t *mapped;	
	} l1;
};

struct proc procs[MAX_PROCS] = { 0 };

static struct pool *frame_pool;

	struct addr_frame *
frame_split(struct addr_frame *f, size_t off)
{
	struct addr_frame *n;

	n = pool_alloc(frame_pool);
	if (n == nil) {
		return nil;
	}

	n->next = nil;
	n->pa = f->pa + off;
	n->len = f->len - off;
	n->table = false;
	n->mapped = 0;

	f->len = off;

	return n;
}

	struct addr_frame *
proc_get_frame(int pid, size_t pa, size_t len)
{
	struct addr_frame *f;

	for (f = procs[pid].frames; f != nil; f = f->next) {
		if (f->pa <= pa && pa < f->pa + f->len) {
			if (f->pa + f->len < pa + len) {
				return nil;
			}

			return f;
		}
	}

	return nil;
}

	int
proc_give_addr(int pid, size_t pa, size_t len)
{
	struct addr_frame *n;

	n = pool_alloc(frame_pool);
	if (n == nil) {
		return ERR;
	}

	n->pa = pa;
	n->len = len;
	n->table = false;
	n->mapped = 0;

	n->next = procs[pid].frames;
	procs[pid].frames = n;

	return OK;
}

	int
proc_take_addr(int pid, size_t pa, size_t len)
{
	struct addr_frame **f, *m;

	for (f = &procs[pid].frames; *f != nil; f = &(*f)->next) {
		if ((*f)->pa == pa && (*f)->len == len) {
			m = *f;

			if (m->mapped > 0 || m->table > 0) {
				return ERR;
			}

			*f = (*f)->next;
			pool_free(frame_pool, m);
			return OK;
		}
	}

	return ERR;
}

	static int
proc_map_table(int pid, 
		size_t pa, size_t va, 
		size_t len, int flags)
{
	struct addr_frame *f;
	uint32_t *addr;
	size_t o;

	f = proc_get_frame(pid, pa, len);
	if (f == nil) {
		return PROC0_ERR_PERMISSION_DENIED;
	}

	if (f->table == 0 && f->mapped > 0) {
		return PROC0_ERR_FLAG_INCOMPATABLE;
	}

	if (f->pa != pa || f->len != len) {
		/* TODO: should allow mappings to parts of frames */
		return ERR;
	}

	addr = map_free(f->pa, f->len, AP_RW_RW, false);
	if (addr == nil) {
		return PROC0_ERR_INTERNAL;
	}

	if (f->table == 0) {
		memset(addr, 0, f->len);
	}

	for (o = 0; (o << 10) < f->len; o++) {
		f->table++;

		if (procs[pid].l1.table[L1X(va) + o] != L1_FAULT) {
			return PROC0_ERR_ADDR_DENIED;
		}

		procs[pid].l1.table[L1X(va) + o]
			= (pa + (o << 10)) | L1_COARSE;

		procs[pid].l1.mapped[L1X(va) + o]
			= ((uint32_t) addr) + (o << 10);
	}

	return OK;
}

	static int
proc_map_normal(int pid, 
		size_t pa, size_t va, 
		size_t len, int flags)
{
	struct addr_frame *f;
	uint32_t tex, c, b, o;
	uint32_t *l2_va;
	bool cache;
	int ap;

	f = proc_get_frame(pid, pa, len);
	if (f == nil) {
		return PROC0_ERR_PERMISSION_DENIED;
	}

	if (flags & MAP_RW) {
		ap = AP_RW_RW;
		if (f->table > 0) {
			return PROC0_ERR_FLAG_INCOMPATABLE;
		}
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
		l2_va = (void *) procs[pid].l1.mapped[L1X(va + o)];
		if (l2_va == nil) {
			return PROC0_ERR_TABLE;
		}

		if (l2_va[L2X(va + o)] != L2_FAULT) {
			return PROC0_ERR_ADDR_DENIED;
		}

		f->mapped++;

		l2_va[L2X(va + o)] = (pa + o)
			| L2_SMALL | tex << 6 | ap << 4 | c << 3 | b << 2;
	}

	return OK;
}

	int
proc_unmap_table(int pid,
		size_t va, size_t len)
{
	struct addr_frame *f;
	size_t o, pa;

	for (o = 0; (o << 10) < len; o++) {
		if (procs[pid].l1.mapped[L1X(va) + o] == nil)
			continue;

		pa = L1PA(procs[pid].l1.table[L1X(va) + o]);
		f = proc_get_frame(pid, pa, TABLE_SIZE);
		if (f == nil) {
			return PROC0_ERR_PERMISSION_DENIED;
		}

		f->table--;
		procs[pid].l1.mapped[L1X(va) + o] = nil;
	}

	return OK;
}

	int
proc_unmap_leaf(int pid, 
		size_t va, size_t len)
{
	struct addr_frame *f;
	uint32_t *l2_va;
	size_t o, pa;

	for (o = 0; o < len; o += PAGE_SIZE) {
		l2_va = (uint32_t *) procs[pid].l1.mapped[L1X(va + o)];
		if (l2_va == nil) {
			return PROC0_ERR_TABLE;
		}

		pa = L2PA(l2_va[L2X(va + o)]);

		f = proc_get_frame(pid, pa, PAGE_SIZE);
		if (f == nil) {
			return PROC0_ERR_PERMISSION_DENIED;
		}

		f->mapped--;
		l2_va[L2X(va + o)] = L2_FAULT;
	}

	return OK;
}

	int
proc_map(int pid, 
		size_t pa, size_t va, 
		size_t len, int flags)
{
	int type;

	/* Don't let procs map past kernel */
	if (info->kernel_pa <= va + len) {
		return PROC0_ERR_ADDR_DENIED;
	}

	type = flags & MAP_TYPE_MASK;
	switch (type) {
		case MAP_REMOVE_TABLE:
			return proc_unmap_table(pid, va, len);

		case MAP_REMOVE_LEAF:
			return proc_unmap_leaf(pid, va, len);

		case MAP_TABLE:
			return proc_map_table(pid, pa, va, len, flags);

		case MAP_MEM:
		case MAP_DEV:
			return proc_map_normal(pid, pa, va, len, flags);

		case MAP_SHARED:
			return ERR;

		default:
			return ERR;
	}
}

	int
init_bundled_proc(char *name,
		size_t start, size_t len)
{
	uint32_t m[MESSAGE_LEN/sizeof(uint32_t)] = { 0 };
	size_t l1_pa, l2_pa, stack_pa, code_pa;
	uint32_t *l1_va, *code_va, *start_va;
	int pid;

	code_pa = get_ram(len, 0x1000);
	if (code_pa == nil) {
		raise();
	}

	code_va = map_free(code_pa, len, AP_RW_RW, false);
	if (code_va == nil) {
		raise();
	}

	start_va = map_free(start, len, AP_RW_RW, true);
	if (start_va == nil) {
		raise();
	}

	memcpy(code_va, start_va, len);

	unmap(code_va, len);
	unmap(start_va, len);

	l1_pa = get_ram(0x8000, 0x4000);
	if (l1_pa == nil) {
		raise();
	}

	l1_va = map_free(l1_pa, 0x8000, AP_RW_RW, false);
	if (l1_va == nil) {
		raise();
	}

	memset(l1_va, 0, 0x8000);
	memcpy(&l1_va[L1X(info->kernel_pa)], 
			&info->proc0.l1_va[L1X(info->kernel_pa)],
			(0x1000 - L1X(info->kernel_pa)) * sizeof(uint32_t));

	l2_pa = get_ram(0x1000, 0x1000);
	if (l2_pa == nil) {
		raise();
	}

	stack_pa = get_ram(0x2000, 0x1000); 
	if (stack_pa == nil) {
		raise();
	}

	pid = proc_new();
	if (pid < 0) {
		raise();
	}

	procs[pid].l1.table = l1_va;
	procs[pid].l1.mapped = &l1_va[0x1000];
	procs[pid].l1.pa = l1_pa;
	procs[pid].l1.len = 0x8000;

	if (proc_give_addr(pid, l2_pa, 0x1000) != OK) {
		raise();
	}

	if (proc_give_addr(pid, stack_pa, 0x2000) != OK) {
		raise();
	}

	if (proc_give_addr(pid, code_pa, len) != OK) {
		raise();
	}

	if (proc_map(pid, l2_pa, 
				0, 0x1000, 
				MAP_TABLE) != OK) {
		raise();
	}

	if (proc_map(pid, stack_pa, 
				USER_ADDR - 0x2000, 0x2000, 
				MAP_MEM|MAP_RW) != OK) {
		raise();
	}

	if (proc_map(pid, code_pa, 
				USER_ADDR, len, 
				MAP_MEM|MAP_RW) != OK) {
		raise();
	}

	if (va_table(pid, procs[pid].l1.pa) != OK) {
		raise();
	}

	m[0] = USER_ADDR;
	m[1] = USER_ADDR;

	send(pid, (uint8_t *) m);

	return pid;
}

	void
init_procs(void)
{
	size_t off;
	int i;

	frame_pool = pool_new(sizeof(struct addr_frame));
	if (frame_pool == nil) {
		raise();
	}

	off = info->bundle_pa;
	for (i = 0; i < nbundled_procs; i++) {
		init_bundled_proc(bundled_procs[i].name,
				off,
				bundled_procs[i].len);

		off += bundled_procs[i].len;
	}

	board_init_bundled_drivers(off);
}


