#include "../proc1/head.h"
#include "../bundle.h"
#include "../dev.h"
#include <arm/mmu.h>

/* Interrupts from daughterboard start at 32?
	 so pl111 is pic[44] -> int 76 
 */

struct device devices[] = {

	{ "sysreg", "arm/drivers/sysreg-sp810",
		0x10000000, 0x1000,
		0
	},

	{ "sd0", "arm/drivers/mmc-pl18x",
		0x10005000, 0x1000,
		41
	},

	{ "serial0", "arm/drivers/serial-pl01x",
		0x10009000, 0x1000,
		37
	},

	{ "eth0", "arm/drivers/ethernet-lan9118",
		0x4e000000, 0x10000,
		15
	},

	{ "lcd0", "arm/drivers/video-pl111",
		0x10020000, 0x1000,
		76
	},

	{ "net0", "arm/drivers/virtio/net",
		0x10013000 + 0x200 * 3, 0x200,
		32 + 40 + 3,
	},

	{ "timer0", "arm/drivers/timer-sp804",
		0x10011000, 0x1000,
		32 + 2,
	},

	{ "timer1", "arm/drivers/timer-sp804",
		0x10012000, 0x1000,
		32 + 3,
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
	struct addr_frame *f;
	int b, d, pid;

	for (b = 0; b < nbundled_drivers; b++) {
		for (d = 0; d < LEN(devices); d++) {
			if (!strcmp(devices[d].compatable, bundled_drivers[b].name))
				continue;

			/* The proc can handle this device */

			pid = init_bundled_proc(devices[d].name,
					2,
					off, 
					bundled_drivers[b].len);

			f = frame_new(PAGE_ALIGN_DN(devices[d].reg), 
					PAGE_ALIGN(devices[d].len));

			proc_give_addr(pid, f);

			init_m[0] = devices[d].reg;
			init_m[1] = devices[d].len;
			init_m[2] = devices[d].irqn;

			uint8_t rp[MESSAGE_LEN];
			mesg(pid, init_m, rp);	
		
			strlcpy((char *) init_m, devices[d].name, sizeof(init_m));	
			mesg(pid, init_m, rp);
		}

		off += bundled_drivers[b].len;
	}
}

