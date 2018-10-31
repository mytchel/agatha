#include "../../sys/head.h"
#include "../kern/fns.h"
#include <stdarg.h>
#include <arm/aml_meson8_uart.h>

static volatile struct aml_meson8_uart_regs *regs;

  static void
putc(char c)
{
	if (c == '\n')
		putc('\r');

	while ((regs->status & UART_STATUS_TX_FULL))
		;

	regs->wfifo = c;  
}

  static void
puts(const char *c)
{
  while (*c)
    putc(*c++);
}

  void
init_aml_uart(size_t regs_pa)
{
	size_t regs_len = 6<<2;

	regs = (void *) kernel_map(regs_pa, regs_len, AP_RW_NO, false);

	debug_puts = &puts;

	debug("kernel aml_meson UART0-AO ready\n");
}

