#include "../../../kern/head.h"
#include "../fns.h"
#include <stdarg.h>
#include <am335x/uart.h>
#include <fdt.h>

static struct uart_regs *regs;
static size_t regs_pa, regs_len;

  static void
putc(char c)
{
  if (c == '\n')
    putc('\r');
	
	while ((regs->lsr & (1 << 5)) == 0)
		;
	
	regs->hr = c;
}

static void
omap_puts(const char *c)
{
  while (*c)
    putc(*c++);
}

	void
init_omap3_uart(void)
{
	debug("kernel omap3-uart ready at 0x%h -> 0x%h\n", regs_pa, regs);
}

static bool
cb(void *dtb, void *node, void *arg)
{
	if (!fdt_node_regs(dtb, node, 0, &regs_pa, &regs_len)) {
		return true;
	}

  regs = (struct uart_regs *) kernel_va_slot;
  map_pages(kernel_l2, regs_pa, kernel_va_slot, regs_len, AP_RW_NO, false);
  kernel_va_slot += PAGE_ALIGN(regs_len);

	fdt_node_path(dtb, node,
			(char **) &kernel_devices.debug_fdt_path,
			sizeof(kernel_devices.debug_fdt_path));

	kernel_devices.debug = &omap_puts;
	kernel_devices.init_debug = &init_omap3_uart;

	return false;
}

void
map_omap3_uart(void *dtb)
{
	if (kernel_devices.debug != nil) {
		return;
	}

	fdt_find_node_compatable(dtb, "ti,omap3-uart",
			&cb, nil);
	
	if (kernel_devices.debug != nil) {
		return;
	}

	fdt_find_node_compatable(dtb, "ti,am3352-uart",
			&cb, nil);
}

