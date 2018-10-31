#include "../../sys/head.h"
#include "../kern/fns.h"
#include "../kern/trap.h"

void init_gic_systick(size_t base);
void init_aml_uart(size_t base);

void
init_kernel_drivers(void)
{
	init_gic_systick(0xC4300000);
	init_aml_uart(0xc81004c0);
}

