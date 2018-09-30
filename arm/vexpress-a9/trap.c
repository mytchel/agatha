#include "../../sys/head.h"
#include "../kern/fns.h"
#include "../kern/trap.h"
#include <arm/gic.h>
#include <arm/cortex_a9_pt_wd.h>


void gic_get_intc_gt(size_t base);

void
get_intc(void)
{
	gic_get_intc_gt(0xC4300000);
}

