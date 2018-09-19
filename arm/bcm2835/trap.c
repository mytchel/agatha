#include "../../port/head.h"
#include "../kern/fns.h"
#include "../kern/trap.h"

static void (*kernel_handlers[256])(size_t) = { nil };
static proc_t user_handlers[256] = { nil };

  int
add_kernel_irq(size_t irqn, void (*func)(size_t))
{
  return ERR;
}

int
add_user_irq(size_t irqn, proc_t p)
{
	debug("add user intr %i -> %i\n", irqn, p->pid);
  return ERR;
}

  void
irq_handler(void)
{

}

	void
get_intc(void)
{

}

void
set_systick(size_t ms)
{
}

void
get_systick(void)
{

}


