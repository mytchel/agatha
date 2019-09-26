#include "../../sys/head.h"
#include "../kern/fns.h"
#include "../kern/trap.h"
#include "../kern/intr.h"
#include <arm/cortex_a9_gic.h>
#include <arm/cortex_a9_pt_wd.h>

/* maximum. will probably be lower as in dst_init */
#define nirq 1020

void (*kernel_handlers[nirq])(size_t) = { nil };

struct user_handler {
	bool setup;
	bool active;
	endpoint_t *e;
	uint32_t signal;
};

struct user_handler user_handlers[nirq] = { { false } };

static volatile struct gic_dst_regs *dregs;
static volatile struct gic_cpu_regs *cregs;
static volatile struct cortex_a9_pt_wd_regs *pt_regs;

	void
gic_set_group(size_t irqn, uint32_t g)
{
	dregs->group[irqn / 32] &= 1 << (irqn % 32);
	dregs->group[irqn / 32] |= g << (irqn % 32);
}

	void
gic_set_priority(size_t irqn, uint8_t p)
{
	((uint8_t *) &dregs->ipriority[irqn / 4])[irqn % 4] = p;
}

	void
gic_set_target(size_t irqn, uint8_t t)
{
	((uint8_t *) &dregs->itargets[irqn / 4])[irqn % 4] = t;
}

	void
irq_disable(size_t irqn)
{
	dregs->icenable[irqn / 32] = 1 << (irqn % 32);
}

	void
irq_enable(size_t irqn)
{
	dregs->isenable[irqn / 32] = 1 << (irqn % 32);
}

	void
irq_clear(size_t irqn)
{
	dregs->icpend[irqn / 32] = 1 << (irqn % 32);
}

	int
irq_ack(size_t irqn)
{
	user_handlers[irqn].active = false;
	cregs->eoi = irqn;

	return OK;
}

	int
irq_add_kernel(size_t irqn, void (*func)(size_t))
{
	kernel_handlers[irqn] = func;

	irq_enable(irqn);

	return OK;
}

	int
irq_add_user(size_t irqn, endpoint_t *e, uint32_t signal)
{
	if (user_handlers[irqn].setup) {
		return ERR;
	}

	user_handlers[irqn].setup = true;
	user_handlers[irqn].e = e;
	user_handlers[irqn].signal = signal;
	user_handlers[irqn].active = false;

	debug_info("adding irq %i for endpoint %i with signal 0x%x\n",
			irqn, e->id, signal);

	irq_enable(irqn);

	return OK;
}

	void
irq_handler(void)
{
	uint32_t irqn = cregs->ack;

	debug_info("irq: %i\n", irqn);

	if (irqn == 1023) {
		debug_warn("interrupt de-asserted before we could handle it!");
		return;
	}

	if (kernel_handlers[irqn] != nil) {
		debug_sched("kernel int %i\n", irqn);
		kernel_handlers[irqn](irqn);

	} else if (user_handlers[irqn].setup) {
		if (user_handlers[irqn].active) {
			debug_warn("got irq %i while still handling it!\n", irqn);
		}

		user_handlers[irqn].active = true;

		debug_warn("signaling\n");
		signal(user_handlers[irqn].e, user_handlers[irqn].signal);

	} else {
		debug_info("got unhandled interrupt %i!\n", irqn);
		irq_disable(irqn);
	}
}

	static void
gic_dst_init(void)
{
	uint32_t irqn, i;

	dregs->control &= ~1;

	irqn = 32 * ((dregs->type & 0x1f) + 1);

	for (i = 0; i < irqn; i++) {
		irq_disable(i);
		gic_set_priority(i, 0);
		gic_set_target(i, 1);
		gic_set_group(i, 0);
	}

	dregs->control |= 1;
}

	static void
gic_cpu_init(void)
{
	cregs->control |= 1;
	cregs->bin_pt = 0;
	cregs->priority = 0xff;
}

static size_t systick_set;

	void
set_systick(size_t t)
{
	systick_set = t;

	pt_regs->t_load = systick_set;
	pt_regs->t_control |= 1;
}

	size_t
systick_passed(void)
{
	return systick_set - pt_regs->t_count;
}

	static void 
systick(size_t irq)
{
	pt_regs->t_intr = 1;

	irq_ack(irq);

	schedule(nil);
}

static void
pt_wd_init(void)
{
	/* Enable interrupt, no prescaler. */
	pt_regs->t_control = (1<<2);

	irq_add_kernel(29, &systick);

	/* Disable watch dog timer. */
	pt_regs->wd_disable = 0x12345678;
	pt_regs->wd_disable = 0x87654321;
}

	void
init_cortex_a9_gic_systick(size_t base)
{
	size_t regs_pa, regs_len, regs;

	regs_pa = base;
	regs_len = 0x2000;

	regs = (size_t) kernel_map(regs_pa, regs_len,
			AP_RW_NO, false);

	cregs = (struct gic_cpu_regs *) 
		(regs + 0x100);

	dregs = (struct gic_dst_regs *) 
		(regs + 0x1000);

	pt_regs = (struct cortex_a9_pt_wd_regs *) 
		(regs + 0x600);

	gic_dst_init();
	gic_cpu_init();
	pt_wd_init();
}

