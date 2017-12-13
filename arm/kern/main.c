#include "../../kern/head.h"
#include "fns.h"
#include <fdt.h>

uint32_t
ttb[4096]__attribute__((__aligned__(0x4000))) = { 0 };

uint32_t
l2[256]__attribute__((__aligned__(0x1000))) = { 0 };

extern uint32_t *_kernel_start;
extern uint32_t *_kernel_end;

uint32_t *kernel_l2;
size_t kernel_va_slot;

  void
main(size_t kernel_start, 
    size_t dtb, 
    size_t dtb_len)
{
  size_t kernel_len;
  proc_t p0;

  /* Early uart. */
  init_uart((void *) 0x44E09000);

  debug("kernel_start = 0x%h\n", kernel_start);

  fdt_get_memory((void *) dtb);

  kernel_len = PAGE_ALIGN((size_t) &_kernel_end - (size_t) &_kernel_start);
  split_out_mem(kernel_start, kernel_len);

  map_l2(ttb, (size_t) l2, kernel_start);

  kernel_va_slot = kernel_start;

  map_pages(l2, kernel_start, kernel_va_slot, 
      kernel_len, AP_RW_NO, true);

  kernel_va_slot += kernel_len;

  map_devs((void *) dtb);

  debug("load and enable mmu\n");

  mmu_load_ttb(ttb);
  mmu_enable();

  init_devs();

  debug("init procs\n");

  p0 = init_procs();

  debug("give proc0 remaining ram\n");
  give_remaining_ram(p0);

  debug("start!\n");

  schedule(p0);
}


