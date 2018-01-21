#include "../../../kern/head.h"
#include "../fns.h"
#include <stdarg.h>

struct timer_regs {
	uint32_t a;
};

static struct timer_regs *regs;

void
map_am335x_timer_1ms(void *dtb)
{
  size_t pa_regs, len;

  pa_regs = 0x44e31000;
  len = 0x2000;

  regs = (struct timer_regs *) kernel_va_slot;
  map_pages(kernel_l2, pa_regs, kernel_va_slot, len, AP_RW_NO, false);
  kernel_va_slot += len;
}

void
init_am335x_timer_1ms(void)
{

}

