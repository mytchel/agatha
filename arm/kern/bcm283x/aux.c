#include "../../../kern/head.h"
#include "../fns.h"
#include <stdarg.h>

struct aux_regs {
  uint32_t irq;
  uint32_t enables;
  uint8_t  pad0[0x38];

  struct {
    uint32_t io;
    uint32_t ier;
    uint32_t iir;
    uint32_t lcr;
    uint32_t mcr;
    uint32_t lsr;
    uint32_t msr;
    uint32_t scratch;
    uint32_t cntl;
    uint32_t stat;
    uint32_t baud;
    uint8_t pad1[0x14];
  } mu;

  struct {
    uint32_t cntrl0;
    uint32_t cntrl1;
    uint32_t stat;
    uint8_t pad0[1];
    uint32_t io;
    uint32_t peek;
    uint8_t pad1[0x28];
  } spi[2];
};

static struct aux_regs *aux = nil;

static void
putc(char c)
{
  while ((aux->mu.stat & (1<<1)) == 0)
    ;

  raise();
  aux->mu.io = c;
}

static void
puts(char *s)
{
  while (*s)
    putc(*s++);
}

  void
map_bcm2835_aux(void *dtb)
{
  size_t pa_regs, len;

  pa_regs = 0x7e215000;
  len = 0x1000;

  aux = (struct aux_regs *) kernel_va_slot;
  map_pages(kernel_l2, pa_regs, kernel_va_slot, len, AP_RW_NO, false);
  kernel_va_slot += len;
}

  void
init_bcm2835_aux(void)
{
  if (aux == nil) {
    return;
  }

  puts("kernel bcm uart ready!\n");
}

