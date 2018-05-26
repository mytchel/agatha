#include "../../kern/head.h"
#include "../kern/fns.h"

void
map_intc(void);

void
init_intc(void);

void
map_serial(void);

void
init_serial(void);

void
init_timer(void);

	void
map_devs(void)
{
  map_serial();
  map_intc();
}

  void
init_devs(void)
{
  init_serial();
  init_intc();
  init_timer();
}

