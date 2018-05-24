#include <types.h>

#include "../proc0/dev.h"

struct device devices[] = {
	{ "serial0",
		"ti,am3352-uart",
		0x44e09000,
		0x2000,
		0x48,
	},

	{ "mmc0",
		"ti,omap4-hsmmc",
		0x48060000,
		0x1000,
		0x40,
	},
	{ "mmc1",
		"ti,omap4-hsmmc",
		0x481d8000,
		0x1000,
		0x1c,
	},
	{ "mmc2",
		"ti,omap4-hsmmc",
		0x47810000,
		0x1000,
		0x1d,
	},

	{ "wdt",
		"ti,omap3-wdt",
		0x44e35000,
		0x1000,
		0x5b,
	},
};
