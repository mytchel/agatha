#include "../../sys/head.h"
#include "../kern/fns.h"
#include "../kern/trap.h"
#include "../kern/intr.h"
#include <arm/cortex_a15_gic.h>

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

	void
gic_set_group(size_t irqn, uint32_t g)
{
	dregs->igroup[irqn / 32] &= 1 << (irqn % 32);
	dregs->igroup[irqn / 32] |= g << (irqn % 32);
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

	void
irq_ack(size_t irqn)
{
	user_handlers[irqn].active = false;
	cregs->eoi = irqn;
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
	if (irqn >= nirq) {
		return ERR;
	} else if (user_handlers[m->irqn].setup) {
		return ERR;
	}

	user_handlers[m->irqn].setup = true;
	user_handlers[m->irqn].e = e;
	user_handlers[m->irqn].signal = signal;
	user_handlers[irqn].active = false;

	debug_info("adding irq %i for endpoint %i with signal 0x%x\n",
			irqn, e->id, signal);

	irq_enable(m->irqn);

	return OK;
}

	int
irq_remove_user(size_t irqn)
{
	user_handlers[irqn].setup = false;
	irq_disable(irqn);

	return OK;
}

	void
irq_handler(void)
{
	uint32_t irqn = cregs->ack;

	debug_info("irq: %i\n", irqn);

	if (irqn == 1023) {
		debug_warn("interrupt de-asserted before we could handle it!\n");
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
		irq_send(user_handlers[irqn].e, user_handlers[irqn].signal);
	} else {
		debug_warn("disabling irq %i!\n", irqn);
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

	debug_info("gic version = %i\n", (dregs->iid >> 12) & 0xf);
	debug_info("ncpu = %i\n", ((dregs->type >> 5) & 7) + 1);
}

	static void
gic_cpu_init(void)
{
	cregs->control |= 1;
	cregs->bin_pt = 0;
	cregs->priority = 0xff;
}

	void
init_cortex_a15_gic(size_t base)
{
	cregs = kernel_map(base + 0x10000, 0x2000,
			AP_RW_NO, false);

	dregs = kernel_map(base, 0x2000,
			AP_RW_NO, false);

	gic_dst_init();
	gic_cpu_init();
}

