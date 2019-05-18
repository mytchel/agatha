#include "../../sys/head.h"
#include "../kern/fns.h"
#include "../kern/trap.h"

void init_cortex_a15_gic(size_t base);
void init_cortex_a15_systick(size_t base);
void init_pl01x(size_t base);

void
init_kernel_drivers(void)
{
	init_pl01x(0x09000000);
	init_cortex_a15_gic(0x08000000);
/*	init_cortex_a15_systick(0x08000000);*/
}

