#include "../../sys/head.h"
#include "../kern/fns.h"
#include "../kern/trap.h"
#include "../kern/intr.h"
#include <arm/cortex_a8_intr.h>

#define nirq 128

void (*kernel_handlers[nirq])(size_t) = { nil };
struct irq_handler user_handlers[nirq] = { { false } };

static volatile struct cortex_a8_intr_regs *regs;

	void
irq_disable(size_t irqn)
{
	regs->bank[irqn / 32].mir_set = 1 << (irqn % 32);
}

void
irq_enable(size_t irqn)
{
	regs->bank[irqn / 32].mir_clear = 1 << (irqn % 32);
}

void
irq_clear(size_t irqn)
{
	irq_disable(irqn);
}

void
irq_end(size_t irqn)
{
	irq_enable(irqn);
}

	void
irq_handler(void)
{
	uint32_t irqn;

	irqn = regs->sir_irq;

	irq_clear(irqn);

	/* Allow new interrupts */
	regs->control = 1;

	if (kernel_handlers[irqn] != nil) {
		debug_sched("kernel int %i\n", irqn);
		kernel_handlers[irqn](irqn);

	} else if (irq_enter(&user_handlers[irqn]) == OK) {
		schedule(nil);

	} else {
		debug_warn("got unhandled interrupt %i!\n", irqn);
		irq_disable(irqn);
	}
}

	int
irq_add_kernel(void (*func)(size_t), size_t irqn)
{
	kernel_handlers[irqn] = func;
	irq_enable(irqn);

	return OK;
}

	void
irq_clear_kernel(size_t irqn)
{
	irq_end(irqn);
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
init_cortex_a8_intr(size_t base)
{
	int i;

	regs = kernel_map(base, 0x1000, AP_RW_NO, false);
	if (regs == nil) {
		panic("failed to map cortex_a8_intr!\n");
	}

	/* enable interface auto idle */
	regs->sysconfig = 1;

	/* mask all interrupts. */
	for (i = 0; i < nirq / 32; i++) {
		regs->bank[i].mir = 0xffffffff;
	}

	/* Set all interrupts to lowest priority. */
	for (i = 0; i < nirq; i++) {
		regs->ilr[i] = 63 << 2;
	}

	regs->control = 1;	
}

