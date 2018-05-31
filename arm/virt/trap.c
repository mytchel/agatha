#include "../../port/head.h"
#include "../kern/fns.h"
#include "../kern/trap.h"
#include <arm/cortex_a9_gic.h>
#include <arm/cortex_a9_pt_wd.h>

static struct cortex_a9_gic_dst_regs *dregs;
static struct cortex_a9_gic_cpu_regs *cregs;
static struct cortex_a9_pt_wd_regs *pt_regs;

static void (*kernel_handlers[256])(size_t) = { nil };
static proc_t user_handlers[256] = { nil };

static void
gic_set_priority(size_t irqn, uint32_t p)
{
	uint8_t *f = (uint8_t *) &dregs->ipr[irqn / 4] + (irqn % 4);
	*f = p;
}

/*
static uint8_t
gic_get_priority(size_t irqn)
{
	uint8_t *f = (uint8_t *) &dregs->ipr[irqn / 4] + (irqn % 4);
	return (*f) & 0xff;
}
*/

static void
gic_set_target(size_t irqn, size_t t)
{
	uint8_t *f = (uint8_t *) &dregs->spi[irqn / 4] + (irqn % 4);
	*f = t;
}

/*
	static uint8_t
gic_get_target(size_t irqn, size_t t)
{
	uint8_t *f = (uint8_t *) &dregs->spi[irqn / 4] + (irqn % 4);
	return (*f) & 0xff;
}
*/

static void
gic_disable_irq(size_t irqn)
{
	dregs->ice[irqn / 32] = 1 << (irqn % 32);
}

static void
gic_enable_irq(size_t irqn)
{
	dregs->ise[irqn / 32] = 1 << (irqn % 32);
}

static void
gic_clear_pending(size_t irqn)
{
	dregs->icp[irqn / 32] = 1 << (irqn % 32);
}

static void
gic_end_interrupt(size_t irqn)
{
	cregs->eoi = irqn;
}

static void
gic_dst_init(void)
{
	uint32_t nirq, i;

	dregs->dcr &= ~1;

	nirq = 32 * ((dregs->ictr & 0x1f) + 1);

	debug("nirq = %i\n", nirq);

	for (i = 32; i < nirq; i++) {
		gic_disable_irq(i);
		gic_set_priority(i, 0xff / 2);
		gic_set_target(i, 0xf);
	}

	/* May have to set icr values here. */

	dregs->dcr |= 1;
}

	static void
gic_cpu_init(void)
{
	uint32_t i;

	cregs->control &= ~1;

	for (i = 0; i < 32; i++) {
		gic_disable_irq(i);
		gic_set_priority(i, 0xff / 2);
	}

	cregs->control |= 1;

	cregs->bin_pt = 0;
	cregs->priority = 0xff;
}

  int
add_kernel_irq(size_t irqn, void (*func)(size_t))
{
	debug("add kernel intr %i\n", irqn);

	gic_enable_irq(irqn);

	kernel_handlers[irqn] = func;

  return ERR;
}

int
add_user_irq(size_t irqn, proc_t p)
{
	debug("add user intr %i -> %i\n", irqn, p->pid);

	if (user_handlers[irqn] == nil) {
		user_handlers[irqn] = p;
		return OK;
	} else {
		return ERR;
	}
}

  void
irq_handler(void)
{
	uint32_t irqn = cregs->ack;

	debug("irq: %i\n", irqn);

	gic_clear_pending(irqn);

	if (kernel_handlers[irqn] != nil) {
		kernel_handlers[irqn](irqn);

	} else if (user_handlers[irqn] != nil) {
		gic_disable_irq(irqn);
		if (send_intr(user_handlers[irqn], irqn) != OK) {
			debug("proc %i not ready for interrupt %i!\n", 
					user_handlers[irqn]->pid, irqn);
		}

	} else {
		debug("got unhandled interrupt %i!\n", irqn);
		gic_disable_irq(irqn);
	}
	
	gic_end_interrupt(irqn);
}

	void
get_intc(void)
{
	size_t regs_pa, regs_len, regs;

	regs_pa = 0x1e000000;
	regs_len = 0x2000;

	regs = (size_t) kernel_map(regs_pa, regs_len,
			AP_RW_NO, false);

	cregs = (struct cortex_a9_gic_cpu_regs *) 
		(regs + 0x100);

	dregs = (struct cortex_a9_gic_dst_regs *) 
		(regs + 0x1000);

	pt_regs = (struct cortex_a9_pt_wd_regs *) 
		(regs + 0x600);

	gic_dst_init();
	gic_cpu_init();
}

void
set_systick(size_t ms)
{
	pt_regs->t_load = ms * 100000;
	pt_regs->t_control |= 1;
}

static void 
systick(size_t irq)
{
	debug("systick\n");
	pt_regs->t_intr = 1;
	schedule(nil);
}

void
get_systick(void)
{
	if (pt_regs == nil) {
		panic("Private timer regs not mapped!\n");
	}

	/* Enable interrupt, no prescaler. */
	pt_regs->t_control = (1<<2);

	add_kernel_irq(29, &systick);

	/* Disable watch dog timer. */
	pt_regs->wd_disable = 0x12345678;
	pt_regs->wd_disable = 0x87654321;
}


