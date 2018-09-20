#include <types.h>

#include "../dev.h"

struct device devices[] = {
	{ "mem0", "mem",
		0x60000000, 0x20000000,
		0		
	},

	/* Devices */
	{ "sysreg", "sp810",
		0x10000000, 0x1000,
		0
	},
		{ "mmc0", "pl19x",
		0x10005000, 0x1000,
		41
	},
	{ "timer0", "t804",
		0x10011000, 0x1000,
		34	
	},
	{ "timer1", "t804",
		0x10012000, 0x1000,
		35
	},
	{ "serial0", "pl01x",
		0x10009000, 0x1000,
		37
	},
};

size_t ndevices = sizeof(devices)/sizeof(devices[0]);

