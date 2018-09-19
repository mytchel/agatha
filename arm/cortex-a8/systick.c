#include "../../port/head.h"
#include "../kern/fns.h"
#include <stdarg.h>

struct timer_regs {
  uint32_t tidr;
  uint8_t pad0[12];
  uint32_t tiocp_cfg;
  uint8_t pad1[12];
  uint32_t irq_eoi;
  uint32_t irqstatus_raw;
  uint32_t irqstatus;
  uint32_t irqenable_set;
  uint32_t irqenable_clr;
  uint32_t irqwakeen;
  uint32_t tclr;
  uint32_t tcrr;
  uint32_t tldr;
  uint32_t ttgr;
  uint32_t twps;
  uint32_t tmar;
  uint32_t tcar1;
  uint32_t tsicr;
  uint32_t tcar2;
};

static struct timer_regs *regs = nil;
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

