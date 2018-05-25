#include "../../kern/head.h"
#include "../kern/fns.h"
#include <stdarg.h>
#include <fdt.h>
#include <arm/sp804.h>

static struct sp804_regs *regs;
static size_t regs_pa, regs_len;
static size_t irqn;

void
set_systick(size_t ms)
{
	regs->t[0].load = ms * 1000;
	regs->t[0].control |= SP804_TIMER_ENABLE;
}

static void
systick(size_t irq)
{
	regs->t[0].intclr = 0;
	debug("got systick\n");
	schedule(nil);
}

  void
init_timer(void)
{
	debug("init timer 0\n");
	
	regs->t[0].control = 
		SP804_TIMER_32BIT | 
		SP804_TIMER_INT_ENABLE | 
		SP804_TIMER_PERIODIC;

	add_kernel_irq(irqn, &systick);
}

  void
map_timer(void)
{
	regs_pa = 0x10000000 + (17 << 12);
	regs_len = 1 << 12;
	irqn = 34;

	regs = (struct sp804_regs *) kernel_va_slot;
	map_pages(kernel_l2, regs_pa, kernel_va_slot, regs_len, AP_RW_NO, false);
	kernel_va_slot += PAGE_ALIGN(regs_len);
}

