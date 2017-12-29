#include "../../kern/head.h"
#include "fns.h"
#include <fdt.h>

struct driver {
  void (*map)(void *dtb);
  void (*init)(void);
};

void
map_ti_am33xx_intc(void *dtb);

void
init_ti_am33xx_intc(void);

void
map_ti_am335x_uart(void *dtb);

void
init_ti_am335x_uart(void);

void
map_bcm2835_aux(void *dtb);

void
init_bcm2835_aux(void);


static struct driver drivers[] = {
  { map_ti_am33xx_intc,
    init_ti_am33xx_intc },
  { map_ti_am335x_uart,
    init_ti_am335x_uart },

  { map_bcm2835_aux,
    init_bcm2835_aux },
};

  void
map_devs(void *dtb)
{
  int i;

  for (i = 0; i < sizeof(drivers)/sizeof(drivers[0]); i++) {
    drivers[i].map(dtb);
  }
}

  void
init_devs(void)
{
  int i;

  for (i = 0; i < sizeof(drivers)/sizeof(drivers[0]); i++) {
    drivers[i].init();
  }
}

