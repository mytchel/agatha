#include "head.h"
#include "../bundle.h"

struct proc procs[MAX_PROCS] = { {-1, 0} };

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

/* TODO: This will have some problems.
	 If a frame is split then the entire thing is
	 mapped it should, but this code will not do that.
	 */
struct addr_frame *
proc_get_frame(int pid, size_t pa, size_t len)
{
	struct addr_frame **f, *m, *n;

	for (f = &procs[pid].frames; *f != nil; f = &(*f)->next) {
		if ((*f)->pa <= pa && pa < (*f)->pa + (*f)->len) {
			if ((*f)->pa + (*f)->len < pa + len) {
				return nil;
			}

			if ((*f)->pa < pa) {
				m = frame_split(*f, pa - (*f)->pa);
				if (m == nil) {
					return nil;
				}
			} else {
				m = *f;
			}

			if (len < m->len) { 
				n = frame_split(m, len);
				n->next = m->next;
				m->next = n;
			}

			return m;
		}
	}

	return nil;
}

int
proc_give(size_t pid, size_t pa, size_t len)
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

	/* Should order this. */
	n->next = procs[pid].frames;
	procs[pid].frames = n;

	return OK;
}

static int
proc_map_table(size_t pid, struct addr_frame *f, size_t va)
{
	void *addr;
	size_t o;

	if (f->table == 0 && f->mapped > 0) {
		return ERR;
	}

	addr = map_free(f->pa, f->len, AP_RW_RW, false);
	if (addr == nil) {
		return ERR;
	}

	if (f->table == 0) {
		f->table = 1;
		memset(addr, 0, f->len);
	} else {
		f->table++;
	}

	map_l2(procs[pid].l1.addr, f->pa, va, f->len);

	for (o = 0; (o << PAGE_SHIFT) < f->len; o++) {
		procs[pid].l1.addr[L1X(va + (o << PAGE_SHIFT)) + 0x1000]
			= ((uint32_t) addr + (o << PAGE_SHIFT));
	}

	return OK;
}

static int
proc_map_normal(size_t pid, struct addr_frame *f, size_t va, int flags)
{
	void *addr;
	bool cache;
	int ap;

	if (flags & MAP_RW) {
		ap = AP_RW_RW;
		if (f->table > 0) {
			return ERR;
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
		return ERR;
	}

	addr = (void *) procs[pid].l1.addr[L1X(va) + 0x1000];
	if (addr == nil) {
		return ERR;
	}

	f->mapped++;

	return map_pages(addr, f->pa, va, f->len, ap, cache);
}

	int
proc_map(size_t pid, 
		size_t pa, size_t va, 
		size_t len, int flags)
{
	struct addr_frame *frame;

	/* For now don't let mappings cross boundaries. */
	if (L1X(va) != L1X(va + len - 1)) {
		return ERR;

	} else if (info->kernel_pa <= va + len) {
		return ERR;
	}

	frame = proc_get_frame(pid, pa, len);
	if (frame == nil) {
		return ERR;
	}

	if ((flags & MAP_TYPE_MASK) == MAP_TABLE) {
		return proc_map_table(pid, frame, va);
	} else { 
		return proc_map_normal(pid, frame, va, flags);
	}
}

	static bool
init_bundled_proc(char *name,
		size_t start, size_t len)
{
	uint32_t m[MESSAGE_LEN/sizeof(uint32_t)] = { 0 };
	size_t l1_pa, l2_pa, stack_pa;
	uint32_t *l1_va, *stack_v;
	int pid;

	l1_pa = get_ram(0x8000, 0x4000);
	if (l1_pa == nil) {
		return false;
	}

	l1_va = map_free(l1_pa, 0x8000, AP_RW_RW, false);
	if (l1_va == nil) {
		return false;
	}

	memset(l1_va, 0, 0x8000);
	memcpy(&l1_va[L1X(info->kernel_pa)], 
			&info->proc0.l1_va[L1X(info->kernel_pa)],
			(0x1000 - L1X(info->kernel_pa)) * sizeof(uint32_t));

	l2_pa = get_ram(0x1000, 0x1000);
	if (l2_pa == nil) {
		return false;
	}

	stack_pa = get_ram(0x2000, 0x1000); 
	if (stack_pa == nil) {
		return false;
	}

	stack_v = map_free(stack_pa, 0x2000, AP_RW_RW, false);
	memset(stack_v, 0, 0x2000);
	unmap(stack_v, 0x2000);

	pid = proc_new();
	if (pid < 0) {
		return false;
	}

	procs[pid].pid = pid;
	procs[pid].l1.addr = l1_va;
	procs[pid].l1.pa = l1_pa;
	procs[pid].l1.len = 0x8000;

	if (proc_give(pid, l1_pa, 0x4000) != OK) {
		return false;
	}

	if (proc_give(pid, l2_pa, 0x1000) != OK) {
		return false;
	}

	if (proc_give(pid, stack_pa, 0x2000) != OK) {
		return false;
	}

	if (proc_give(pid, start, len) != OK) {
		return false;
	}

	if (proc_map(pid, l2_pa, 
				0, 0x1000, 
				MAP_TABLE) != OK) {
		return false;
	}

	if (proc_map(pid, stack_pa, 
				USER_ADDR - 0x2000, 0x2000, 
				MAP_MEM|MAP_RW) != OK) {
		return false;
	}

	if (proc_map(pid, start, 
				USER_ADDR, len, 
				MAP_MEM|MAP_RW) != OK) {
		return false;
	}

	if (va_table(pid, procs[pid].l1.pa) != OK) {
		procs[pid].pid = -1;
		return false;
	}

	m[0] = USER_ADDR;
	m[1] = USER_ADDR;

	return send(pid, (uint8_t *) m) == OK;
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
		if (!init_bundled_proc(bundled_procs[i].name, 
					off, bundled_procs[i].len)) {
			raise();
		}

		off += bundled_procs[i].len;
	}
}

