#include "../proc0/head.h"
#include "../bundle.h"
#include "../dev.h"
#include <arm/mmu.h>

struct device devices[] = 
{
	{ "serial0", "arm/drivers/serial-pl01x",
		0x09000000, 0x1000,
		37,
	},

	{ "sd0", "arm/drivers/virtio/blk",
		0x0a003e00, 0x200,
		32 + 16 + 31,
	},

	{ "gpu0", "arm/drivers/virtio/gpu",
		0x0a003c00, 0x200,
		32 + 16 + 30,
	},

	{ "net0", "arm/drivers/virtio/net",
		0x0a003a00, 0x200,
		32 + 16 + 29,
	},

	{ "key0", "arm/drivers/virtio/input",
		0x0a003800, 0x200,
		32 + 16 + 28,
	},

	{ "mouse0", "arm/drivers/virtio/input",
		0x0a003600, 0x200,
		32 + 16 + 27,
	},

};

	void
board_init_ram(void)
{
	add_ram(0x40000000, 0x20000000);
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
		
			send(pid, init_m);	
		
			strlcpy((char *) init_m, devices[d].name, sizeof(init_m));	
			send(pid, init_m);	
		}

		off += bundled_drivers[b].len;
	}
}

