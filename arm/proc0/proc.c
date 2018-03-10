#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <fdt.h>

#include "proc0.h"
#include "../kern/mem.h"

struct bundle_proc {
  char name[256];
  size_t len;
};

struct bundle_proc bundled_procs[] = {
#include "bundle.list"
};

extern void *_binary_arm_proc0_bundle_bin_start;

static bool
init_bundled_proc(char *name,
    size_t start, size_t len)
{
  uint32_t m[MESSAGE_LEN/sizeof(uint32_t)] = { 0 };
	size_t stack_p, stack_v, slen;
	size_t l1_p, l1_v, l2_p, l2_v;
  int pid;

	stack_p = 0x80000000;
	slen        = 0x1000;

	l1_p    = 0x80010000;
	l2_p    = 0x80015000;

	stack_v    = 0x14000;
	l1_v       = 0x15000;
	l2_v       = 0x19000;

	map_pages((uint32_t *) 0x5000, stack_p, stack_v, slen,
			AP_RW_RW, true);

	map_pages((uint32_t *) 0x5000, l1_p, l1_v, 0x4000,
			AP_RW_RW, true);

	map_pages((uint32_t *) 0x5000, l2_p, l2_v, 0x1000,
			AP_RW_RW, true);

	yield();

	/* Hard copy l1 address for now */
	memcpy((uint32_t *) l1_v, (uint32_t *) 0x1000, 0x4000);

	memset((uint32_t *) l2_v, 0, 0x1000);
	memset((uint32_t *) stack_v, 0, slen);

	map_l2((uint32_t *) l1_v, l2_p, 0);

	map_pages((uint32_t *) l2_v, start, USER_ADDR, len,
			AP_RW_RW, true);

	map_pages((uint32_t *) l2_v, stack_p, USER_ADDR - slen, slen,
			AP_RW_RW, true);

	unmap_pages((uint32_t *) 0x5000, stack_v, slen);
	unmap_pages((uint32_t *) 0x5000, l1_v, 0x4000);
	unmap_pages((uint32_t *) 0x5000, l2_v, 0x1000);

	pid = proc_new();
	if (pid < 0) {
		return false;
	}

	if (va_table(pid, l1_p) != OK) {
		return false;
	}

	m[0] = USER_ADDR;
	m[1] = USER_ADDR;

	if (send(pid, (uint8_t *) m) == OK) {
		return true;
	} else {
		return false;
	}
}

	bool
init_procs(void)
{
	size_t off;
	int i;

	off = ((size_t) &_binary_arm_proc0_bundle_bin_start)
		- USER_ADDR + 0x82020000;
	if (off == nil) {
		return false;
	}

	va_table(10, off);

	for (i = 0; 
			i < sizeof(bundled_procs)/sizeof(bundled_procs[0]);
			i++) {

		if (!init_bundled_proc(bundled_procs[i].name, 
					off, bundled_procs[i].len)) {
			return false;
		}

		off += bundled_procs[i].len;
	}

	return true;
}

