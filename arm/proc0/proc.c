#include "head.h"
#include <arm/mmu.h>
#include "../bundle.h"
#include <log.h>

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

struct proc procs[MAX_PROCS] = { 0 };

static uint8_t frame_pool_initial[sizeof(struct pool_frame)
	+ (sizeof(struct addr_frame) + sizeof(struct pool_obj)) * 1024];

static struct pool frame_pool;

	struct addr_frame *
frame_new(size_t pa, size_t len)
{
	struct addr_frame *n;

	n = pool_alloc(&frame_pool);
	if (n == nil) {
		return nil;
	}

	n->next = nil;
	n->pa = pa;
	n->len = len;
	n->table = false;
	n->mapped = 0;

	return n;
}

void
frame_free(struct addr_frame *f)
{
	release_addr(f->pa, f->len);

	pool_free(&frame_pool, f);
}

	int
proc_give_addr(int pid, struct addr_frame *f)
{
	f->table = false;
	f->mapped = 0;

	f->next = procs[pid].frames;
	procs[pid].frames = f;

	return OK;
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

struct addr_frame *
proc_take_addr(int pid, size_t pa, size_t len)
{
	struct addr_frame **f, *m;

	for (f = &procs[pid].frames; *f != nil; f = &(*f)->next) {
		if ((*f)->pa == pa && (*f)->len == len) {
			m = *f;

			if (m->mapped > 0 || m->table > 0) {
				return nil;
			}

			*f = (*f)->next;
			return m;
		}
	}

	return nil;
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

	addr = map_addr(f->pa, f->len, MAP_RW|MAP_DEV);
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
	if (info->kernel_va <= va + len) {
		log(LOG_INFO, "0x%x + 0x%x is beyond kernel 0x%x",
				va, len, info->kernel_va);
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
		int priority,
		size_t start, size_t len)
{
	union proc_msg m;

	size_t code_pa, stack_pa;
	uint32_t *code_va, *start_va;
	
	size_t l1_table_pa, l1_mapped_pa;
	uint32_t *l1_table_va, *l1_mapped_va;

	size_t l2_pa;

	struct addr_frame *f;
	int pid, r;

	log(LOG_INFO, "kernel_info at 0x%x, kernel_start at 0x%x",
			info, info->kernel_va);
	log(LOG_INFO, "init bundled proc %s", name);

	log(LOG_INFO, "get ram for code copy");	
	code_pa = get_ram(len, 0x1000);
	if (code_pa == nil) {
		exit(1);
	}

	log(LOG_INFO, "code copy at 0x%x, find map", code_pa);

	code_va = map_addr(code_pa, len, MAP_RW|MAP_DEV);
	if (code_va == nil) {
		log(LOG_INFO, "map code 0x%x failed", code_pa);
		exit(2);
	}

	start_va = map_addr(start, len, MAP_RO|MAP_DEV);
	if (start_va == nil) {
		exit(3);
	}

	log(LOG_INFO, "kernel_info at 0x%x, kernel_start at 0x%x",
			info, info->kernel_va);
	log(LOG_INFO, "copy 0x%x from 0x%x / 0x%x to 0x%x / 0x%x",
			len, start, start_va, code_pa, code_va);

	memcpy(code_va, start_va, len);

	log(LOG_INFO, "unmap code");
	if ((r = unmap_addr(code_va, len)) != OK) {
		exit(r);
	}

	log(LOG_INFO, "unmap start");
	if ((r = unmap_addr(start_va, len)) != OK) {
		exit(r);
	}
	
	log(LOG_INFO, "kernel_info at 0x%x, kernel_start at 0x%x",
			info, info->kernel_va);
	log(LOG_INFO, "get l1");

	l1_table_pa = get_ram(0x4000, 0x4000);
	if (l1_table_pa == nil) {
		exit(4);
	}

	l1_mapped_pa = get_ram(0x4000, 0x1000);
	if (l1_mapped_pa == nil) {
		exit(4);
	}

	log(LOG_INFO, "kernel_info at 0x%x, kernel_start at 0x%x",
			info, info->kernel_va);
	l1_table_va = map_addr(l1_table_pa, 0x4000, MAP_RW|MAP_DEV);
	if (l1_table_va == nil) {
		exit(5);
	}

	l1_mapped_va = map_addr(l1_mapped_pa, 0x4000, MAP_RW|MAP_DEV);
	if (l1_mapped_va == nil) {
		exit(5);
	}

	log(LOG_INFO, "kernel_info at 0x%x, kernel_start at 0x%x",
			info, info->kernel_va);

	log(LOG_INFO, "memset mapped l1 0x%x", l1_mapped_va);
	memset(l1_mapped_va, 0, 0x4000);
	
	log(LOG_INFO, "kernel_info at 0x%x, kernel_start at 0x%x",
			info, info->kernel_va);
	
	log(LOG_INFO, "copy table l1 0x%x", l1_table_va);
	proc_init_l1(l1_table_va);
	
	log(LOG_INFO, "kernel_info at 0x%x, kernel_start at 0x%x",
			info, info->kernel_va);

	pid = proc_new(l1_table_pa, 0, priority);
	if (pid < 0) {
		exit(8);
	}

	procs[pid].l1.table = l1_table_va;
	procs[pid].l1.mapped = l1_mapped_va;
	procs[pid].l1.table_pa = l1_table_pa;
	procs[pid].l1.mapped_pa = l1_mapped_pa;

	log(LOG_INFO, "kernel_info at 0x%x, kernel_start at 0x%x",
			info, info->kernel_va);
	/* Initial l2 */

	l2_pa = get_ram(0x1000, 0x1000);
	if (l2_pa == nil) {
		exit(6);
	}

	if ((f = frame_new(l2_pa, 0x1000)) == nil) {
		exit(0x9);
	}

	if ((r = proc_give_addr(pid, f)) != OK) {
		exit(0xa);
	}

	log(LOG_INFO, "kernel_info at 0x%x, kernel_start at 0x%x",
			info, info->kernel_va);
	if ((r = proc_map(pid, l2_pa, 
				0, 0x1000, 
				MAP_TABLE)) != OK) {
		exit(r);
	}

	/* Stack */

	log(LOG_INFO, "kernel_info at 0x%x, kernel_start at 0x%x",
			info, info->kernel_va);
	size_t stack_len = 0x2000;
	stack_pa = get_ram(stack_len, 0x1000); 
	if (stack_pa == nil) {
		exit(7);
	}

	if ((f = frame_new(stack_pa, stack_len)) == nil) {
		exit(0xb);
	}

	if ((r = proc_give_addr(pid, f)) != OK) {
		exit(0xc);
	}

	log(LOG_INFO, "kernel_info at 0x%x, kernel_start at 0x%x",
			info, info->kernel_va);
	if ((r = proc_map(pid, stack_pa, 
				USER_ADDR - stack_len, stack_len, 
				MAP_MEM|MAP_RW)) != OK) {
		exit(0x10);
	}

	log(LOG_INFO, "kernel_info at 0x%x, kernel_start at 0x%x",
			info, info->kernel_va);
	/* Code */

	if ((f = frame_new(code_pa, len)) == nil) {
		exit(0xd);
	}

	if ((r = proc_give_addr(pid, f)) != OK) {
		exit(0xe);
	}

	log(LOG_INFO, "kernel_info at 0x%x, kernel_start at 0x%x",
			info, info->kernel_va);
	if ((r = proc_map(pid, code_pa, 
				USER_ADDR, len, 
				MAP_MEM|MAP_RW)) != OK) {
		exit(0x11);
	}

	log(LOG_INFO, "kernel_info at 0x%x, kernel_start at 0x%x",
			info, info->kernel_va);

	m.start.type = PROC_start_msg;
	m.start.pc = USER_ADDR;
	m.start.sp = USER_ADDR;

	log(LOG_INFO, "start bundled proc pid %i %s", pid, name);

	log(LOG_INFO, "kernel_info at 0x%x, kernel_start at 0x%x",
			info, info->kernel_va);

	send(pid, (uint8_t *) &m);

	log(LOG_INFO, "kernel_info at 0x%x, kernel_start at 0x%x",
			info, info->kernel_va);

	return pid;
}

	void
init_procs(void)
{
	size_t off;
	int i;

	if (pool_init(&frame_pool, sizeof(struct addr_frame)) != OK) {
		exit(1);
	}

	if (pool_load(&frame_pool, frame_pool_initial, sizeof(frame_pool_initial)) != OK) {
		exit(1);
	}

	off = info->bundle_pa;
	for (i = 0; i < nbundled_procs; i++) {
		init_bundled_proc(bundled_procs[i].name,
				1,
				off,
				bundled_procs[i].len);

		off += bundled_procs[i].len;
	}

	board_init_bundled_drivers(off);
}


