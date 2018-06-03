#include "../../port/head.h"
#include "../kern/fns.h"
#include "../kern/trap.h"

#define nirq 128

struct intc {
	uint32_t revision;
	uint32_t pad1[3];
	uint32_t sysconfig;
	uint32_t sysstatus;
	uint32_t pad2[10];
	uint32_t sir_irq;
	uint32_t sir_fiq;
	uint32_t control;
	uint32_t protection;
	uint32_t idle;
	uint32_t pad3[3];
	uint32_t irq_priority;
	uint32_t fiq_priority;
	uint32_t threshold;
	uint32_t pad4[5];

	struct {
		uint32_t itr;
		uint32_t mir;
		uint32_t mir_clear;
		uint32_t mir_set;
		uint32_t isr_set;
		uint32_t isr_clear;
		uint32_t pending_irq;
		uint32_t pending_fiq;
	} set[4];

	uint32_t ilr[nirq];
};

static struct intc *intc = nil;

static void (*kernel_handlers[nirq])(size_t) = { nil };

	static void
mask_intr(uint32_t irqn)
{
	uint32_t mask, mfield;

	mfield = irqn / 32;
	mask = 1 << (irqn % 32);

	intc->set[mfield].mir_set = mask;
}

	static void
unmask_intr(uint32_t irqn)
{
	uint32_t mask, mfield;

	mfield = irqn / 32;
	mask = 1 << (irqn % 32);

	intc->set[mfield].mir_clear = mask;
}

	int
add_kernel_irq(size_t irqn, void (*func)(size_t))
{
	kernel_handlers[irqn] = func;

	unmask_intr(irqn);

	return OK;
}

	int
add_user_irq(size_t irqn, proc_t p)
{
	return ERR;
}

	void
irq_handler(void)
{
	uint32_t irq;

	irq = intc->sir_irq;

	debug("interrupt %i\n", irq);

	if (kernel_handlers[irq]) {
		kernel_handlers[irq](irq);

	} else {
		mask_intr(irq);
	}

	/* Allow new interrupts. */
	intc->control = 1;
}

	void
get_intc(void)
{
	size_t addr, size;
	int i;

	addr = 0x48200000;
	size = 0x1000;

	intc = kernel_map(addr, size, AP_RW_NO, false);

	intc->control = 1;

	/* enable interface auto idle */
	intc->sysconfig = 1;

	/* mask all interrupts. */
	for (i = 0; i < 4; i++) {
		intc->set[i].mir = 0xffffffff;
	}

	/* Set all interrupts to lowest priority. */
	for (i = 0; i < nirq; i++) {
		intc->ilr[i] = 63 << 2;
	}

	intc->control = 1;
}

