#include "../../kern/head.h"
#include "fns.h"
#include <fdt.h>

struct driver {
  void (*map)(void *dtb);
  void (*init)(void);
};

void
map_ti_am33xx_intc(void *dtb);

void
map_omap3_uart(void *dtb);

void
map_am335x_timer(void *dtb);

static struct driver drivers[] = {
	{ map_omap3_uart },
	{ map_ti_am33xx_intc },
	{ map_am335x_timer },
};

struct kernel_devices kernel_devices = { nil };

	void
map_devs(void *dtb)
{
	int i;

	for (i = 0; i < sizeof(drivers)/sizeof(drivers[0]); i++) {
		drivers[i].map(dtb);
	}

	if (kernel_devices.trap == nil ||
			kernel_devices.add_kernel_irq == nil ||
			kernel_devices.add_user_irq == nil ||
			kernel_devices.timer == nil ||
			kernel_devices.debug == nil) {

		/* We have a problem! */
		raise();
	}

	memcpy(kernel_info.intc_fdt_path,
			kernel_devices.intc_fdt_path,
			sizeof(kernel_info.intc_fdt_path));

	memcpy(kernel_info.systick_fdt_path,
			kernel_devices.systick_fdt_path,
			sizeof(kernel_info.systick_fdt_path));

	memcpy(kernel_info.debug_fdt_path,
			kernel_devices.debug_fdt_path,
			sizeof(kernel_info.debug_fdt_path));

}

	void
init_devs(void)
{
	kernel_devices.init_debug();
	kernel_devices.init_intc();
	kernel_devices.init_timer();
}

	void
trap(size_t pc, int type)
{
	debug("trap\n");
	kernel_devices.trap(pc, type);
}

	void
set_systick(size_t ms)
{
	kernel_devices.timer(ms);
}

