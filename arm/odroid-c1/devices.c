#include "../proc0/head.h"
#include "../bundle.h"
#include "../dev.h"

#define IO_CBUS_BASE  0xc1100000
#define CBUS_REG_OFFSET(reg) ((reg) << 2)
#define CBUS_REG_ADDR(reg) (IO_CBUS_BASE + CBUS_REG_OFFSET(reg))

struct device devices[] = {
	{ "timer-ae", "drivers/aml-timer",
		CBUS_REG_ADDR(0x2650), CBUS_REG_OFFSET(6),
		0 },
	{ "timer-fh", "drivers/aml-timer",
		CBUS_REG_ADDR(0x2664), CBUS_REG_OFFSET(4),
		0 },
	{ "timer-ao", "drivers/aml-timer",
		0xc810004c, 12,
		0 },

	{ "watchdog0", "drivers/aml-watchdog",
		CBUS_REG_ADDR(0x2640), 8,
		0 },
	{ "watchdog0-ao", "drivers/aml-watchdog",
		0xc8100044, 8,
		0 },

	{ "rtc0", "drivers/aml-rtc",
		0xc8100740, 20,
		0 },

	{ "sdmmc0", "drivers/mmc-aml",
		 0xc1108e00, 48, 
/*		0xc1108c20, 48, */
		0 },

	{ "i2c0", "drivers/aml-i2c",
		CBUS_REG_ADDR(0x2140), CBUS_REG_OFFSET(7),
		0 },

	{ "spi0", "drivers/aml-spi",
		0xc1108d80, 40,
		0 },

	/*
	{ "uart0", "drivers/serial-aml",
		CBUS_REG_ADDR(0x2130), CBUS_REG_OFFSET(6),
		0 },
	{ "uart1", "drivers/serial-aml",
		CBUS_REG_ADDR(0x2137), CBUS_REG_OFFSET(6),
		0 },
	{ "uart2", "drivers/serial-aml",
		CBUS_REG_ADDR(0x21c0), CBUS_REG_OFFSET(6),
		0 },
		*/
	{ "uart0-ao", "drivers/serial-aml",
		0xc81004c0, 24,
		0 },
	/*
	{ "uart2-ao", "drivers/serial-aml",
		0xc81004e0, 24,
		0 },
*/	

	{ "eth0", "drivers/aml-mac",
		CBUS_REG_ADDR(0x2050), CBUS_REG_OFFSET(2),
		0 },
	
	{ "dma0", "drivers/aml-ndma",
		CBUS_REG_ADDR(0x2270), CBUS_REG_OFFSET(10),
		0 },
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

			pid = init_bundled_proc(devices[d].name,
					off, 
					bundled_drivers[b].len);
			
			proc_give_addr(pid, 
					PAGE_ALIGN_DN(devices[d].reg), 
					PAGE_ALIGN(devices[d].len));

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

