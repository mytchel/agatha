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
init_ti_am33xx_intc(void);

void
map_omap3_uart(void *dtb);

void
init_omap3_uart(void);

void
map_am335x_timer(void *dtb);

void
init_am335x_timer(void);

static struct driver drivers[] = {
	{ map_omap3_uart,
		init_omap3_uart },
	{ map_ti_am33xx_intc,
		init_ti_am33xx_intc },
	{ map_am335x_timer,
		init_am335x_timer },
};

struct kernel_devices kernel_devices = { nil };

	void
map_devs(void *dtb)
{
	int i;

	for (i = 0; i < sizeof(drivers)/sizeof(drivers[0]); i++) {
		drivers[i].map(dtb);
	}
}

	void
init_devs(void)
{
	int i;

	for (i = 0; i < sizeof(drivers)/sizeof(drivers[0]); i++) {
		drivers[i].init();
	}

	if ( kernel_devices.trap == nil ||
			kernel_devices.add_kernel_irq == nil ||
			kernel_devices.add_user_irq == nil ||
			kernel_devices.timer == nil ||
			kernel_devices.debug == nil) {

		/* We have a problem! */
		raise();
	}
}

void
trap(size_t pc, int type)
{
	kernel_devices.trap(pc, type);
}

