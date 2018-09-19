#include "../../port/head.h"
#include "../kern/fns.h"
#include <stdarg.h>
#include <arm/omap3_uart.h>

static struct omap3_uart_regs *regs;
static size_t regs_pa, regs_len;

  static void
putc(char c)
{
  if (c == '\n')
    putc('\r');

  while ((regs->lsr & (1 << 5)) == 0)
    ;

  regs->hr = c;
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
  regs_pa = 0x44e09000;
  regs_len = 0x2000;

  regs = kernel_map(regs_pa, regs_len, AP_RW_NO, false);
  
	debug("kernel omap3-uart ready at 0x%h -> 0x%h\n", regs_pa, regs);
}

