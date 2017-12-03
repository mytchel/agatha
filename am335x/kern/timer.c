#include <head.h>
#include "fns.h"

struct timer {
	uint32_t tidr;
	uint32_t pad1[3];
	uint32_t tiocp_cfg;
	uint32_t pad2[3];
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

struct cm_dpll {
	uint32_t pad1[1];
	uint32_t timer7_clk;
	uint32_t timer2_clk;
	uint32_t timer3_clk;
	uint32_t timer4_clk;
	uint32_t mac_clksel;
	uint32_t timer5_clk;
	uint32_t timer6_clk;
	uint32_t cpts_rtf_clksel;
	uint32_t timer1ms_clk;
	uint32_t gfx_fclk;
	uint32_t pru_icss_ocp_clk;
	uint32_t lcdc_pixel_clk;
	uint32_t wdt1_clk;
	uint32_t gpio0_bdclk;
};

struct watchdog {
	uint32_t widr;
	uint32_t pad1[3];
	uint32_t wdsc;
	uint32_t wdst;
	uint32_t wisr;
	uint32_t wier;
	uint32_t pad2[1];
	uint32_t wclr;
	uint32_t wcrr;
	uint32_t wldr;
	uint32_t wtgr;
	uint32_t wwps;
	uint32_t pad3[3];
	uint32_t wdly;
	uint32_t wspr;
	uint32_t pad4[2];
	uint32_t wirqstatraw;
	uint32_t wirqstat;
	uint32_t wirqenset;
	uint32_t wireqenclr;
};

static int t_irq;
static struct timer *timer2;
static struct cm_dpll *cm; 
static struct watchdog *wdt;

static void systick_handler(uint32_t);

void
init_watchdog(void *regs)
{
 	wdt = regs;
 
  /* Disable watchdog timer. */
  
	wdt->wspr = 0x0000AAAA;
  while (wdt->wwps & (1<<4))
    ;

	wdt->wspr = 0x00005555;
  while (wdt->wwps & (1<<4))
    ;
}

void
init_timers(void *t_regs, int _t_irq, void *c_regs)
{
	timer2 = (struct timer *) t_regs;
	t_irq = _t_irq;
	
	cm = (struct cm_dpll *) c_regs;
	
  /* Select 32KHz clock for timer 2 */
  cm->timer2_clk = 2;

  /* set irq for overflow */
	timer2->irqenable_set = 1<<1;

  intc_add_handler(t_irq, &systick_handler);
}

void
systick_handler(uint32_t irqn)
{
	debug("handle systick\n");
	
  /* Clear irq status if it is set. */
  timer2->irqstatus = 1<<1;

  intc_reset();
  
  debug("schedule from 0x%h\n", up);
  schedule(nil);
}

uint32_t
ms_to_ticks(uint32_t ms)
{
  return ms * 32;
}

void
set_systick(uint32_t ms)
{
  /* set timer */
  timer2->tcrr = 0xffffffff - ms_to_ticks(ms);
  
  /* start timer */
  timer2->tclr = 1;
}


