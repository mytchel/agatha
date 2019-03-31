#include "../../sys/head.h"
#include "../kern/fns.h"
#include "../kern/trap.h"
#include <arm/am335x_dmtimer.h>

static volatile struct am335x_dmtimer_regs *regs;
static size_t time_set;

	void
set_systick(size_t t)
{
	time_set = 0xffffffff - t;

	regs->tcrr = time_set;
	regs->tclr = 1;
}

size_t
systick_passed(void)
{
	uint32_t t;

	regs->irqstatus = 1<<1;
	
	t = regs->tcrr - time_set;

	return t;
}

static void 
systick(size_t irq)
{
	regs->irqstatus = 1<<1;
	irq_clear();

	schedule(nil);
}

	void
init_am335x_systick(size_t base)
{
	regs = kernel_map(base, 0x5c, AP_RW_NO, false);

	/* set irq for overflow */
	regs->irqenable_set = 1<<1;

	/* May have to set the timer speed with CM_DPLL */

	add_kernel_irq(66, &systick);
}

