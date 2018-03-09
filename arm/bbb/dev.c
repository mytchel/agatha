#include "../../kern/head.h"
#include "../kern/fns.h"
#include <fdt.h>

void
map_intc(void);

void
init_intc(void);

void
map_uart(void);

void
init_uart(void);

void
map_timer(void);

void
init_timer(void);

	void
map_devs(void)
{
  map_intc();
  map_uart();
  map_timer();
}

  void
init_devs(void)
{
  init_intc();
  init_uart();
  init_timer();
}

