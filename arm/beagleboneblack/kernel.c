#include "../../sys/head.h"
#include "../kern/fns.h"
#include "../kern/trap.h"

void init_am335x_systick(size_t base);
void init_cortex_a8_intr(size_t base);
void init_omap3_uart(size_t base);

void
init_kernel_drivers(void)
{
	init_cortex_a8_intr(0x48200000);
	init_am335x_systick(0x44e05000); 
/*	init_omap3_uart(0x44e09000); */
}

