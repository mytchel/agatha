#include "../../sys/head.h"
#include "../kern/fns.h"
#include <stdarg.h>
#include <arm/aml_meson8_uart.h>

static volatile struct aml_meson8_uart_regs *regs;
static size_t regs_pa, regs_len;

  static void
putc(char c)
{
	if (c == '\n')
		putc('\r');

	while ((regs->status & UART_STATUS_TX_FULL))
		;

	regs->wfifo = c;  
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
	size_t offset, base;
	regs_pa = 0xc81004c0;
	regs_len = 6<<2;

	base = PAGE_ALIGN_DN(regs_pa);
	offset = regs_pa - base;

	regs = (void *) (kernel_map(base, PAGE_ALIGN(regs_len), AP_RW_NO, false)
			+ offset);

	puts("kernel aml_meson UART0-AO ready\n");

	debug("mapped registers 0x%h from 0x%h to 0x%h offset 0x%h\n",
			regs_pa, base, regs, offset);
	debug("status = 0x%h\n", regs->status);
	debug("control= 0x%h\n", regs->control);
}

