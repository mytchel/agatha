#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <fdt.h>

#include "proc0.h"

struct kernel_info *k_info;
void *dtb;

  void
main(struct kernel_info *k)
{
  int f_id;

  k_info = k;

  if (!find_tables()) {
    return;
  }

  f_id = frame_create(k_info->dtb_start, k_info->dtb_len, F_TYPE_MEM);
  if (f_id < 0) {
    return;
  }

  dtb = map_frame(f_id, F_MAP_TYPE_PAGE|F_MAP_READ);
  if (dtb == nil) {
    return;
  }

  if (!setup_mem()) {
    return;
  }

  if (!init_procs()) {
    return;
  }

  while (true)
    ;
}

