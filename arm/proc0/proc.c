#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>

#include "proc0.h"
#include "../bundle.h"

static bool
init_bundled_proc(char *name,
    size_t start, size_t len)
{
  uint32_t m[MESSAGE_LEN/sizeof(uint32_t)] = { 0 };
	void *stack_v, *l1_v, *l2_v;
	size_t stack_p, slen;
	size_t l1_p, l2_p;
  int pid;

	slen = 0x1000;
	
	stack_p = get_mem(slen, 0x1000); 
	if (stack_p == nil) {
		return false;
	}

	l1_p    = get_mem(0x4000, 0x4000);
	if (l1_p == nil) {
		free_mem(stack_p, slen);
		return false;
	}

	l2_p    = get_mem(0x1000, 0x1000);
	if (l1_p == nil) {
		free_mem(l1_p, 0x4000);
		free_mem(stack_p, slen);
		return false;
	}

	stack_v    = map_free(stack_p, slen,
			AP_RW_RW, true);

	l1_v       = map_free(l1_p, 0x4000,
			AP_RW_RW, true);

	l2_v       = map_free(l2_p, 0x1000,
			AP_RW_RW, true);

	init_l1(l1_v);

	memset(l2_v, 0, 0x1000);
	memset(stack_v, 0, slen);

	map_l2(l1_v, l2_p, 0);

	map_pages(l2_v, start, USER_ADDR, len,
			AP_RW_RW, true);

	map_pages(l2_v, stack_p, USER_ADDR - slen, slen,
			AP_RW_RW, true);

	unmap(stack_v, slen);
	unmap(l1_v, 0x4000);
	unmap(l2_v, 0x1000);

	pid = proc_new();
	if (pid < 0) {
		free_mem(l2_p, 0x1000);
		free_mem(l1_p, 0x4000);
		free_mem(stack_p, slen);
		return false;
	}

	if (va_table(pid, l1_p) != OK) {
		free_mem(l2_p, 0x1000);
		free_mem(l1_p, 0x4000);
		free_mem(stack_p, slen);
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

	off = info->bundle_start;

	for (i = 0; i < nbundled_procs; i++) {
		if (!init_bundled_proc(bundled_procs[i].name, 
					off, bundled_procs[i].len)) {
			raise();
		}

		off += bundled_procs[i].len;
	}
}

