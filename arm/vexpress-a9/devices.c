#include "../proc0/head.h"
#include "../bundle.h"
#include "../dev.h"

struct device devices[] = {

	{ "sysreg", "drivers/sysreg-sp810",
		0x10000000, 0x1000,
		0
	},

	{ "sdmmc0", "drivers/mmc-pl18x",
		0x10005000, 0x1000,
		41
	},

	{ "serial0", "drivers/serial-pl01x",
		0x10009000, 0x1000,
		37
	},

	{ "eth0", "drivers/ethernet-lan9118",
		0x4e000000, 0x10000,
		15
	},

	{ "lcd0", "drivers/video-pl111",
		0x10020000, 0x1000,
		44
	},

	{ "lcd1", "drivers/video-pl111",
		0x1001f000, 0x1000,
		14
	},

};

	void
board_init_ram(void)
{
	add_ram(0x60000000, 0x20000000);
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

