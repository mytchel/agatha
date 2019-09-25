#include "../root/head.h"
#include "../bundle.h"
#include <arm/mmu.h>
#include <log.h>

/* Interrupts from daughterboard start at 32?
	 so pl111 is pic[44] -> int 76 
 */

struct device devices[] = {

	{ "sysreg", "arm/drivers/sysreg-sp810",
		0x10000000, 0x1000,
		0,
		0, 0, false,
	},

	{ "sd0", "arm/drivers/mmc-pl18x",
		0x10005000, 0x1000,
		41,
		0, 0, false,
	},

	{ "serial0", "arm/drivers/serial-pl01x",
		0x10009000, 0x1000,
		37,
		0, 0, false,
	},

	{ "eth0", "arm/drivers/ethernet-lan9118",
		0x4e000000, 0x10000,
		15,
		0, 0, false,
	},

	{ "lcd0", "arm/drivers/video-pl111",
		0x10020000, 0x1000,
		76,
		0, 0, false,
	},

	{ "net0", "arm/drivers/virtio/net",
		0x10013000 + 0x200 * 3, 0x200,
		32 + 40 + 3,
		0, 0, false,
	},

	{ "timer0", "arm/drivers/timer-sp804",
		0x10011000, 0x1000,
		32 + 2,
		0, 0, false,
	},

	{ "timer1", "arm/drivers/timer-sp804",
		0x10012000, 0x1000,
		32 + 3,
		0, 0, false,
	},
};

size_t ndevices = LEN(devices);

	void
board_init_ram(void)
{
	add_ram(0x60000000, 0x20000000);
}

	size_t
board_init_bundled_drivers(size_t off)
{
	struct device *dev;
	int b, d;

	for (b = 0; b < nbundled_drivers; b++) {
		for (d = 0; d < ndevices; d++) {
			dev = &devices[d];
			if (!strcmp(dev->compatable, bundled_drivers[b].name))
				continue;

			/* The proc can handle this device */

			log(0, "init device %s", dev->name);
	
			if (!init_bundled_proc(bundled_drivers[b].name, 2, 
					off, bundled_drivers[b].len,
					&dev->pid, &dev->eid))
			{
				continue;
			}
		}

		off += bundled_drivers[b].len;
	}

	return off;
}

