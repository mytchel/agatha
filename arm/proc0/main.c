#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>

#include "proc0.h"

struct kernel_info *info;

  void
main(struct kernel_info *i)
{
  uint8_t m[MESSAGE_LEN];

	info = i;
	
	init_mem();
	init_procs();

  while (true) {
    recv(m);
  }
}

