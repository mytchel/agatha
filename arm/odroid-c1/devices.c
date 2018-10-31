#include "../proc0/head.h"
#include "../bundle.h"
#include "../dev.h"

struct device devices[] = {
};

size_t ndevices = sizeof(devices)/sizeof(devices[0]);

	void
board_init_ram(void)
{
	add_ram(0x01000000, 0x40000000);
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

			add_mmio(devices[d].reg, devices[d].len);

			pid = init_bundled_proc(devices[d].name,
					off, 
					bundled_drivers[b].len);

			init_m[0] = devices[d].reg;
			init_m[1] = devices[d].len;
			init_m[2] = devices[d].irqn;
		
			send(pid, init_m);	
		}

		off += bundled_drivers[b].len;
	}
}

