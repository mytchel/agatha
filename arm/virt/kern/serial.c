#include "../../kern/head.h"
#include "../kern/fns.h"
#include <stdarg.h>
#include <arm/pl01x.h>
#include <fdt.h>

static struct pl01x_regs *regs;
static size_t regs_pa, regs_len;

  static void
putc(char c)
{
	while ((regs->fr & UART_PL01x_FR_TXFF))
		;

	regs->dr = c;  
}

  void
puts(const char *c)
{
  while (*c)
    putc(*c++);
}

  void
init_serial(void)
{
	debug("kernel pl01x ready\n");
}

  void
map_serial(void *dtb)
{
	regs_pa = 0x10000000 + (9 << 12);
	regs_len = 1 << 12;

	regs = (struct pl01x_regs *) kernel_va_slot;
	map_pages(kernel_l2, regs_pa, kernel_va_slot, regs_len, AP_RW_NO, false);
	kernel_va_slot += PAGE_ALIGN(regs_len);
}

