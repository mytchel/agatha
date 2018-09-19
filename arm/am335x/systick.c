#include "../../port/head.h"
#include "../kern/fns.h"
#include <stdarg.h>
#include <arm/omap3_dmtimer.h>

static struct omap3_dmtimer_regs *regs = nil;
static size_t regs_pa, regs_len;
static size_t irqn;

void
set_systick(size_t ms)
{
  regs->tcrr = 0xffffffff - (31250 * ms);

  regs->irqstatus = (1<<1);

  /* Start timer. */
  regs->tclr = (1<<0);
}

  static void
systick(size_t irq)
{
  schedule(nil);
}

	void
get_systick(void)
{
	/* DMTimer2 */

  regs_pa = 0x48040000;
  regs_len = 0x400;
  irqn = 0x43;

  regs = kernel_map(regs_pa, regs_len, AP_RW_NO, false);

	/* Reset and wait */
  regs->tiocp_cfg |= (1<<1);

  while ((regs->tiocp_cfg & (1<<0)))
    ;

  /* Enable interrupt on overflow. */
  regs->irqenable_set = (1<<1);

  add_kernel_irq(irqn, &systick);
}

