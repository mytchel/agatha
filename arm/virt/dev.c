#include "../../kern/head.h"
#include "../kern/fns.h"
#include <fdt.h>

void
map_intc(void);

void
init_intc(void);

void
map_serial(void);

void
init_serial(void);

void
map_timer(void);

void
init_timer(void);

	void
map_devs(void)
{
  map_serial();
  map_intc();
  map_timer();
}

  void
init_devs(void)
{
  init_intc();
  init_serial();
  init_timer();
  jump(0x12340000);
}

