#include "../../kern/head.h"
#include "fns.h"
#include "trap.h"
#include <m.h>

void
fill_intr_m(uint8_t *m, size_t irqn)
{
	struct proc0_irq *i = (struct proc0_irq *) m;

	i->from = PROC0_PID;
	i->type = PROC0_irq;
	i->irq = irqn;
}

