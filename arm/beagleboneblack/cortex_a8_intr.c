#include "../../sys/head.h"
#include "../kern/fns.h"
#include "../kern/trap.h"
#include <arm/cortex_a8_intr.h>

#define nirq 128

static volatile struct cortex_a8_intr_regs *regs;

static void (*kernel_handlers[nirq])(size_t) = { nil };
static proc_t user_handlers[nirq] = { nil };

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
add_kernel_irq(size_t irqn, void (*func)(size_t))
{
  kernel_handlers[irqn] = func;
  unmask_intr(irqn);

	return OK;
}

int
add_user_irq(size_t irqn, proc_t p)
{
	debug(DEBUG_INFO, "add user intr %i -> %i\n", irqn, p->pid);

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
  uint32_t irqn;
	message_t m;
	
  irqn = regs->sir_irq;

  /* Allow new interrupts */
	regs->control = 1;

  if (kernel_handlers[irqn] != nil) {
    /* Kernel handler */
    kernel_handlers[irqn](irqn);
  } else if (user_handlers[irqn] != nil) {

		/* May need to mask interrupt */

		m = message_get();
		if (m != nil) {
			m->from = -1;
			((int *) m->body)[1] = irqn;
			send(user_handlers[irqn], m);
		}
	} else {
		debug(DEBUG_INFO, "got unhandled interrupt %i!\n", irqn);
    mask_intr(irqn);
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

