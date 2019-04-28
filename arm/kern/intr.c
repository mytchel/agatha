#include "../../sys/head.h"
#include "fns.h"
#include "trap.h"
#include "intr.h"

struct irq_handler *active_irq = nil;

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
irq_enter(struct irq_handler *h)
{
	proc_t p;

	if (!h->registered) {
		return ERR;
	}

	p = find_proc(h->map.pid);
	if (p == nil) {
		irq_remove_user(h->map.irqn);
		return ERR;
	}
	
	/* Will need to change if we have priorities */
	h->next = active_irq;
	active_irq = h;

	p->irq_label = &h->label;

	h->exiting = false;

	p->irq_label->psr = MODE_USR;
	p->irq_label->regs[0] = h->map.irqn;

	p->irq_label->regs[1] = 
		(uint32_t) h->map.arg;
	p->irq_label->pc = 
		(uint32_t) h->map.func;
	p->irq_label->sp = 
		(uint32_t) h->map.sp;

	debug_sched("entering irq %i on pid %i\n",
			h->map.irqn, p->pid);

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

			irq_end(active_irq->map.irqn);

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
go_idle(void)
{
	set_intr(0);
	while (true)
		;
}

	void
trap(size_t pc, int type)
{
	uint32_t fsr;
	size_t addr;

	if (up != nil)
		debug_info("trap proc %i\n", up->pid);

	debug_info("trap at 0x%x, type %i\n", pc, type);

	switch(type) {
		case ABORT_INTERRUPT:
			irq_handler();

			return; /* Note the return. */

		case ABORT_INSTRUCTION:
			debug_warn("abort instruction at 0x%x\n", pc);
			break;

		case ABORT_PREFETCH:
			debug_warn("prefetch instruction at 0x%x\n", pc);
			break;

		case ABORT_DATA:
			addr = fault_addr();
			fsr = fsr_status() & 0xf;

			debug_warn("data abort at 0x%x for 0x%x type 0x%x\n", pc, addr, fsr);

			switch (fsr) {
				case 0x5: /* section translation */
				case 0x7: /* page translation */
				case 0x0: /* vector */
				case 0x1: /* alignment */
				case 0x3: /* also alignment */
				case 0x2: /* terminal */
				case 0x4: /* external linefetch section */
				case 0x6: /* external linefetch page */
				case 0x8: /* external non linefetch section */
				case 0xa: /* external non linefetch page */
				case 0x9: /* domain section */
				case 0xb: /* domain page */
				case 0xc: /* external translation l1 */
				case 0xe: /* external translation l2 */
				case 0xd: /* section permission */
				case 0xf: /* page permission */
				default:
					break;
			}

			break;
	}

	if (up == nil) {
		panic("trap with no proc on cpu!!!\n");
	}

	debug_warn("killing proc %i\n", up->pid);

	up->state = PROC_dead;

	schedule(nil);

	/* Never reached */
}


