#include "../../sys/head.h"
#include "../kern/fns.h"
#include "../kern/trap.h"

void init_gic_systick(size_t base);
void init_pl01x(size_t base);

void
init_kernel_drivers(void)
{
	init_gic_systick(0x1e000000);
/*	init_pl01x(0x10000000 + (9 << 12)); */
}

