#include "../../kern/head.h"
#include "fns.h"
#include "trap.h"
#include <m.h>

void
fill_intr_m(uint8_t *m, size_t irqn)
{
	struct msg_irq *i = (struct msg_irq *) m;

	i->type = MSG_T_irq;
	i->irq = irqn;
}

