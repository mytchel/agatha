#include "../../sys/head.h"
#include "../kern/fns.h"
#include <stdarg.h>
#include <arm/omap3_uart.h>

static volatile struct omap3_uart_regs *regs;

  static void
putc(char c)
{
	if (c == '\n')
		putc('\r');

	while ((regs->lsr & (1 << 5)) == 0)
		;

	regs->hr = c;  
}

  static void
puts(const char *c)
{
  while (*c)
    putc(*c++);
}

  void
init_omap3_uart(size_t regs_pa)
{
	size_t regs_len = 0x88;

	regs = (void *) kernel_map(regs_pa, regs_len, AP_RW_NO, false);

	debug_puts = &puts;

	debug(DEBUG_INFO, "kernel omap3_uart UART0 ready\n");
}

