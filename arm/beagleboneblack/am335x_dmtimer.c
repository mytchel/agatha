#include "../../sys/head.h"
#include "../kern/fns.h"
#include "../kern/trap.h"
#include <arm/am335x_dmtimer.h>

static volatile struct am335x_dmtimer_regs *regs;

	void
set_systick(size_t ms)
{
	uint32_t t = ms * 32;
	regs->tcrr = 0xffffffff - t;
	regs->irqstatus = 3;
	regs->tclr = 1;
}

static void 
systick(size_t irq)
{
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

