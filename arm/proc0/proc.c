#include "head.h"
#include "../bundle.h"

struct proc procs[MAX_PROCS] = { 0 };

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
	if (l1 == nil) {
		return nil;
	}

	if (va == nil) {
		va = l1_free_va(l1, len);
	}

	if (info->kernel_pa <= va + len) {
		return nil;
	}

	l2 = l1_get_l2(l1, va);
	if (l2 == nil) {
		l2 = l2_create_table(0x1000, L1X(va));
		if (l2 == nil) {
			return nil;
		}

		if (l1_insert_l2(l1, l2) != OK) {
			l2_free(l2);
			return nil;
		}
	}

	l3 = l3_create(pa, va, len, flags);
	if (l3 == nil) {
		return nil;
	}

	if (l2_insert_l3(l2, l3) != OK) {
		l3_free(l3);
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

	l1 = l1_create();
	if (l1 == nil) {
		return false;
	}

	pid = proc_new();
	if (pid < 0) {
		l1_free(l1);
		return false;
	}

	procs[pid].pid = pid;
	procs[pid].l1 = l1;

	if (va_table(pid, l1->pa) != OK) {
		procs[pid].pid = 0;
		l1_free(l1);
		return false;
	}

	slen = 0x1000;

	stack_p = get_ram(slen, 0x1000); 
	if (stack_p == nil) {
		return false;
	}

	stack_v = map_free(stack_p, slen, MAP_MEM|MAP_RW);
	memset(stack_v, 0, slen);
	unmap(stack_v, slen);

	if (proc_map(pid, stack_p, 
				USER_ADDR - slen, slen, 
				MAP_MEM|MAP_RW) != USER_ADDR - slen) {
		return false;
	}

	if (proc_map(pid, start, 
				USER_ADDR, len, 
				MAP_MEM|MAP_RW) != USER_ADDR) {
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

	off = info->bundle_pa;

	for (i = 0; i < nbundled_procs; i++) {
		if (!init_bundled_proc(bundled_procs[i].name, 
					off, bundled_procs[i].len)) {
			raise();
		}

		off += bundled_procs[i].len;
	}
}

