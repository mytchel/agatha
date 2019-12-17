#include "../root/head.h"
#include "../bundle.h"
#include <arm/mmu.h>
#include <log.h>

/* Interrupts from daughterboard start at 32?
	 so pl111 is pic[44] -> int 76 
 */

struct service services[] = {

	{ 
		"sysreg", "arm/drivers/sysreg-sp810",
		{ true, 0x10000000, 0x1000, 0, false, false },
		{ { 0, nil } },
	},

	{ 
		"serial0", "arm/drivers/serial-pl01x",
		{ true, 0x10009000, 0x1000, 37, false, false },
		{ { 0, nil } },
	},

	
	{ 
		"sd0", "arm/drivers/mmc-pl18x",
		{ true, 0x10005000, 0x1000, 41, false, false },
		{ { RESOURCE_type_log, "log" },
		  { RESOURCE_type_timer, "timer0" },
		  { 0, nil } },
	},

	{ 
		"net0", "arm/drivers/virtio/net",
		{ true, 0x10013000 + 0x200 * 3, 0x200, 32 + 40 + 3, false, false },
		{ { RESOURCE_type_log, "log" }, 
		  { RESOURCE_type_timer, "timer0" },
		  { 0, nil } },
	},

	{ 
		"timer0", "arm/drivers/timer-sp804",
		{ true, 0x10011000, 0x1000, 32 + 2, false, false },
		{ { RESOURCE_type_log, "log" }, 
		  { 0, nil } },
	},
/*
	{ 
		"timer1", "arm/drivers/timer-sp804",
		{ true, 0x10012000, 0x1000, 32 + 3, false, false },
		{ { RESOURCE_type_log, "log" }, 
		  { 0, nil } },
	},
*/
	{
		"log", "procs/log",
		{ false },
		{ { RESOURCE_type_serial, "serial0" },
		  { 0, nil } },
	},

	{
		"test", "procs/test",
		{ false },
		{ { RESOURCE_type_log, "log" },
		  { RESOURCE_type_timer, "timer0" },
		  { RESOURCE_type_block, "sd0" },
		  { RESOURCE_type_net, "net0" },
		  { 0, nil } },
	},
};

size_t nservices = LEN(services);

	void
board_init_ram(void)
{
	add_ram(0x60000000, 0x20000000);
}

