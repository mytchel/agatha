#include "../../kern/head.h"
#include "../kern/fns.h"
#include <stdarg.h>
#include <arm/omap3_uart.h>
#include <fdt.h>

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
init_uart(void)
{
  debug("kernel omap3-uart ready at 0x%h -> 0x%h\n", regs_pa, regs);
}

  void
map_uart(void *dtb)
{
  regs_pa = 0x44e09000;
  regs_len = 0x2000;

  regs = (struct omap3_uart_regs *) kernel_va_slot;
  map_pages(kernel_l2, regs_pa, kernel_va_slot, regs_len, AP_RW_NO, false);
  kernel_va_slot += PAGE_ALIGN(regs_len);
}
