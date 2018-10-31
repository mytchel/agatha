#include "../../sys/head.h"
#include "../kern/fns.h"
#include <stdarg.h>
#include <arm/pl01x.h>

static volatile struct pl01x_regs *regs;

  static void
putc(char c)
{
	while ((regs->fr & UART_PL01x_FR_TXFF))
		;

	regs->dr = c;  
}

  static void
puts(const char *c)
{
  while (*c)
    putc(*c++);
}

  void
init_pl011(size_t regs_pa)
{
	size_t regs_len = 1 << 12;

	regs = kernel_map(regs_pa, regs_len, AP_RW_NO, false);

	debug_puts = &puts;

	debug("kernel pl011 ready\n");
}

