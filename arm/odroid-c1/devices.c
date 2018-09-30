#include <types.h>

#include "../dev.h"

struct device devices[] = {
	{ "mem0", "mem",
		0x01000000, 0x40000000,
		0		
	},

	/* Devices */

};

size_t ndevices = sizeof(devices)/sizeof(devices[0]);

