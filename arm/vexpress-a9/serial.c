#include "../../sys/head.h"
#include "../kern/fns.h"
#include <stdarg.h>
#include <arm/pl01x.h>

static volatile struct pl01x_regs *regs;
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
get_serial(void)
{
	regs_pa = 0x10000000 + (9 << 12);
	regs_len = 1 << 12;

	regs = kernel_map(regs_pa, regs_len, AP_RW_NO, false);

	debug("kernel pl02x ready\n");
}

