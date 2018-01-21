#include "../../../kern/head.h"
#include "../fns.h"
#include <stdarg.h>
#include <am335x/uart.h>

static struct uart_regs *uart;

  static void
putc(char c)
{
  if (c == '\n')
    putc('\r');
	
	while ((uart->lsr & (1 << 5)) == 0)
		;
	
	uart->hr = c;
}

void
serial_puts(const char *c)
{
  while (*c)
    putc(*c++);
}

void
map_omap3_uart(void *dtb)
{
  size_t pa_regs, len;

	/* TODO: Grap registers for all uart devices. */

  pa_regs = 0x44e09000;
  len = 0x2000;

  uart = (struct uart_regs *) kernel_va_slot;
  map_pages(kernel_l2, pa_regs, kernel_va_slot, len, AP_RW_NO, false);
  kernel_va_slot += len;
}

void
init_omap3_uart(void)
{
	if (uart == nil) {
		return;
	}
  
	serial_puts("kernel omap3-uart ready.\n");
}

