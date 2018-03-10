#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <fdt.h>

#include "proc0.h"

  void
main(struct kernel_info *k)
{
  uint8_t m[MESSAGE_LEN];

	init_mem();
	init_procs();

  while (true) {
    recv(m);
  }
}

