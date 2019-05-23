#include "../../sys/head.h"
#include "../kern/fns.h"
#include "../kern/intr.h"

uint64_t timer_read_count(void);
uint64_t timer_read_cmp(void);
uint32_t timer_read_ctrl(void);
uint32_t timer_read_freq(void);

void timer_write_cmp(uint64_t);
void timer_write_ctrl(uint32_t);

static size_t ticks_per_ms;
static uint64_t ticks_set;

static void
systick(size_t irq)
{
	timer_write_ctrl(0);

	irq_end(irq);

	schedule(nil);
}

size_t
systick_passed(void)
{
	size_t t = (size_t) (timer_read_count() - ticks_set);
	return t / ticks_per_ms;
}

void
set_systick(size_t ms)
{
	uint64_t v;

	ticks_set = timer_read_count();	
	v	= ticks_set + ms * ticks_per_ms;
	timer_write_cmp(v);
	timer_write_ctrl(1);
}

	void
init_cortex_a15_systick(void)
{
	ticks_per_ms = timer_read_freq() / 1000;

	irq_add_kernel(&systick, 30);
}


