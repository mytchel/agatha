#include "../../sys/head.h"
#include "../kern/fns.h"
#include "../kern/trap.h"
#include "../kern/intr.h"
#include <arm/cortex_a15_gic.h>

/* maximum. will probably be lower as in dst_init */
#define nirq 1020

void (*kernel_handlers[nirq])(size_t) = { nil };
struct irq_handler user_handlers[nirq] = { { false } };

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
irq_end(size_t irqn)
{
	cregs->eoi = irqn;
}

	int
irq_add_kernel(void (*func)(size_t), size_t irqn)
{
	kernel_handlers[irqn] = func;

	irq_enable(irqn);

	return OK;
}

	int
irq_add_user(struct intr_mapping *m)
{
	if (user_handlers[m->irqn].registered) {
		return ERR;
	}

	memcpy(&user_handlers[m->irqn].map, m,
			sizeof(struct intr_mapping));

	user_handlers[m->irqn].registered = true;

	debug_info("adding irq %i for %i with func 0x%x, arg 0x%x, sp 0x%x\n",
			m->irqn, m->pid, m->func, m->arg, m->sp);

	irq_enable(m->irqn);

	return OK;
}

	int
irq_remove_user(size_t irqn)
{
	user_handlers[irqn].registered = false;
	irq_disable(irqn);

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

	} else if (irq_enter(&user_handlers[irqn]) == OK) {
		schedule(nil);

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

	void
set_systick(size_t t)
{
}

	size_t
systick_passed(void)
{
	return 0;
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

