#include "../../sys/head.h"
#include "../kern/fns.h"
#include "../kern/trap.h"
#include <arm/cortex_a8_intr.h>

#define nirq 128

struct irq_handler {
	bool registered;
	bool exiting;

	struct intr_mapping map;
	label_t label;

	struct irq_handler *next;
};

static volatile struct cortex_a8_intr_regs *regs;

static void (*kernel_handlers[nirq])(size_t) = { nil };
static struct irq_handler user_handlers[nirq] = {
 	{ false } 
};

static struct irq_handler *active_irq = nil;


void
mask_intr(uint32_t irqn)
{
  uint32_t mask, bank;

  bank = irqn / 32;
  mask = 1 << (irqn % 32);

	regs->bank[bank].mir_set = mask;
}

static void
unmask_intr(uint32_t irqn)
{
  uint32_t mask, bank;

  bank = irqn / 32;
  mask = 1 << (irqn % 32);

	regs->bank[bank].mir_clear = mask;
}

int
irq_add_kernel(void (*func)(size_t), size_t irqn)
{
  kernel_handlers[irqn] = func;
  unmask_intr(irqn);

	return OK;
}

	void
irq_clear_kernel(size_t irqn)
{
	unmask_intr(irqn);
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

	unmask_intr(m->irqn);

	return OK;
}

	int
irq_remove_user(size_t irqn)
{
	user_handlers[irqn].registered = false;
	mask_intr(irqn);

	return OK;
}

	int
irq_exit(void)
{
	if (active_irq == nil || 
			active_irq->map.pid != up->pid) {
	
		return ERR;
	}

	active_irq->exiting = true;

	debug_sched("exiting irq %i on pid %i\n",
			active_irq->map.irqn, up->pid);

	schedule(nil);

	return OK;
}

	int
irq_enter(size_t irqn)
{
	proc_t p;

	if (!user_handlers[irqn].registered) {
		return ERR;
	}

	p = find_proc(user_handlers[irqn].map.pid);
	if (p == nil) {
		irq_remove_user(irqn);
		return ERR;
	}

	/* Will need to change if we have priorities */
	user_handlers[irqn].next = active_irq;
	active_irq = &user_handlers[irqn];

	p->irq_label = &user_handlers[irqn].label;

	user_handlers[irqn].exiting = false;

	p->irq_label->psr = MODE_USR;
	p->irq_label->regs[0] = irqn;

	p->irq_label->regs[1] = 
		(uint32_t) user_handlers[irqn].map.arg;
	p->irq_label->pc = 
		(uint32_t) user_handlers[irqn].map.func;
	p->irq_label->sp = 
		(uint32_t) user_handlers[irqn].map.sp;

	debug_sched("entering irq %i on pid %i\n",
			irqn, p->pid);

	return OK;
}

	void
irq_run_active(void)
{
	while (active_irq != nil) {
		if (!active_irq->registered) {
			active_irq = active_irq->next;
			continue;
		}

		up = find_proc(active_irq->map.pid);
		if (up == nil) {
			irq_remove_user(active_irq->map.irqn);

			active_irq = active_irq->next;
			continue;
		} 
		
		if (active_irq->exiting) {
			debug_sched("%i leaving irq\n", up->pid);
			up->in_irq = false;
	
			unmask_intr(active_irq->map.irqn);

			active_irq = active_irq->next;
			continue;
		}

		debug_sched("switch to %i at 0x%x to handle irq %i\n",
				up->pid, active_irq->label.pc, active_irq->map.irqn);

		mmu_switch(up->vspace);
		set_systick(100000);
	
		if (up->in_irq) {	
			debug_sched("re-enter\n");
			goto_label(up->irq_label);
		} else {
			debug_sched("first enter\n");

			up->in_irq = true;
			drop_to_user(up->irq_label);
		}
	}
}

	void
irq_handler(void)
{
	uint32_t irqn;

	irqn = regs->sir_irq;

	mask_intr(irqn);

	/* Allow new interrupts */
	regs->control = 1;

	if (kernel_handlers[irqn] != nil) {
		debug_sched("kernel int %i\n", irqn);
		kernel_handlers[irqn](irqn);

	} else if (irq_enter(irqn) == OK) {
		schedule(nil);

		debug_sched("irq handle %i returned, go back to user\n", irqn);
	} else {
		debug_warn("got unhandled interrupt %i!\n", irqn);
	}
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

