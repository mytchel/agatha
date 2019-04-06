#include "../proc0/head.h"
#include "../bundle.h"
#include "../dev.h"

struct device devices[] = {

	{ "prm-cm0", "drivers/prm-cm-am33xx",
		0x44e00000, 0x2000,
		0
	},

	{ "serial0", "drivers/serial-omap3",
		0x44e09000, 0x1000,
		72	
	},

 	 { "sdmmc0", "drivers/mmc-omap3",
		0x48060000, 0x1000,
		64
	},

	{ "sdmmc1", "drivers/mmc-omap3",
		0x481d8000, 0x1000,
		28
	},

	{ "video0", "drivers/video-am335x",
		0x4830e000, 0x2000,
		36
	},

	{ "i2c0", "drivers/i2c-am335x",
		0x44e0b000, 0x1000,
		70
	},

	{ "i2c1", "drivers/i2c-am335x",
		0x4802a000, 0x1000,
		71	
	},

	{ "i2c2", "drivers/i2c-am335x",
		0x4819c000, 0x1000,
		30	
	},
};

	void
board_init_ram(void)
{
	add_ram(0x80000000, 0x20000000);
}

	void
board_init_bundled_drivers(size_t off)
{
	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];
	int b, d, pid;

	for (b = 0; b < nbundled_drivers; b++) {
		for (d = 0; d < LEN(devices); d++) {
			if (!strcmp(devices[d].compatable, bundled_drivers[b].name))
				continue;

			/* The proc can handle this device */

			pid = init_bundled_proc(devices[d].name,
					off, 
					bundled_drivers[b].len);
			
			proc_give_addr(pid, devices[d].reg, devices[d].len);

			init_m[0] = devices[d].reg;
			init_m[1] = devices[d].len;
			init_m[2] = devices[d].irqn;
		
			send(pid, init_m);	

			strlcpy((char *) init_m, devices[d].name, sizeof(init_m));
			send(pid, init_m);
		}

		off += bundled_drivers[b].len;
	}
}

