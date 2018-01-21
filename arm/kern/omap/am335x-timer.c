#include "../../../kern/head.h"
#include "../fns.h"
#include <stdarg.h>
#include <fdt.h>

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

	static void
am335x_timer(size_t ms)
{
	regs->tcrr = 0xffffffff - (31250 * ms);

	/* Start timer. */
	regs->tclr = (1<<0);
}


static bool
cb(void *dtb, void *node, void *arg)
{
	if (!fdt_node_regs(dtb, node, 0, &regs_pa, &regs_len)) {
		panic("Failed to get ti,am335x-timer regs!\n");
	}
	
  regs = (struct timer_regs *) kernel_va_slot;
  map_pages(kernel_l2, regs_pa, kernel_va_slot, regs_len, AP_RW_NO, false);
  kernel_va_slot += PAGE_ALIGN(regs_len);

	kernel_devices.timer = &am335x_timer;

	/* Don't continue finding more. */	
	return false;
}

void
map_am335x_timer(void *dtb)
{
	if (kernel_devices.timer != nil) {
		return;
	}

	fdt_find_node_compatable(dtb, "ti,am335x-timer",
			&cb, nil);
}

void
init_am335x_timer(void)
{
	if (regs == nil) {
		return;
	}

	/* Reset and wait */
	regs->tiocp_cfg |= (1<<1);

	while ((regs->tiocp_cfg & (1<<0)))
		;

	/* Enable interrupt on overflow. */
	regs->irqenable_set = (1<<1);

	debug("am335x-timer-1ms ready at 0x%h -> 0x%h\n", regs_pa, regs);
}

