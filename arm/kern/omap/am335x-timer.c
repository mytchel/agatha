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
static size_t irqn;

	static void
am335x_timer(size_t ms)
{
	regs->tcrr = 0xffffffff - (31250 * ms);

	regs->irqstatus = (1<<1);

	/* Start timer. */
	regs->tclr = (1<<0);
}

static void
am335x_systick(size_t irq)
{
	schedule(nil);
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

	kernel_devices.add_kernel_irq(irqn, &am335x_systick);

	debug("am335x-timer-1ms ready at 0x%h -> 0x%h : irq %i\n", 
			regs_pa, regs, irqn);
}

static bool
cb(void *dtb, void *node, void *arg)
{
	uint32_t *data;
	int len;

	len = fdt_node_property(dtb, node, "interrupts", (char **) &data);
	if (len != sizeof(uint32_t)) {
		return true;
	}	

	irqn = beto32(*data);

	if (!fdt_node_regs(dtb, node, 0, &regs_pa, &regs_len)) {
		return true;
	}
	
  regs = (struct timer_regs *) kernel_va_slot;
  map_pages(kernel_l2, regs_pa, kernel_va_slot, regs_len, AP_RW_NO, false);
  kernel_va_slot += PAGE_ALIGN(regs_len);

	kernel_devices.init_timer = &init_am335x_timer;
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

