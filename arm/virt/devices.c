#include <types.h>

#include "../dev.h"

struct device devices[] = {
	/* Memory split around kernel */
	{ "mem0", "mem",
		0x60000000, 2000000,
		0		
	},
	{ "mem1", "mem",
		0x63000000, 0x1d000000,
		0
	},

	/* Devices */
	{ "mmc0", "mmc",
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
	{ "wd0", "wd",
		0x1000f000, 0x1000,
		32
	},
};

size_t ndevices = sizeof(devices)/sizeof(devices[0]);

