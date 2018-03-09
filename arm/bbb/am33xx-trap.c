#include "../../kern/head.h"
#include "../kern/fns.h"
#include "../kern/trap.h"
#include <fdt.h>

#define nirq 128

struct intc {
  uint32_t revision;
  uint32_t pad1[3];
  uint32_t sysconfig;
  uint32_t sysstatus;
  uint32_t pad2[10];
  uint32_t sir_irq;
  uint32_t sir_fiq;
  uint32_t control;
  uint32_t protection;
  uint32_t idle;
  uint32_t pad3[3];
  uint32_t irq_priority;
  uint32_t fiq_priority;
  uint32_t threshold;
  uint32_t pad4[5];

  struct {
    uint32_t itr;
    uint32_t mir;
    uint32_t mir_clear;
    uint32_t mir_set;
    uint32_t isr_set;
    uint32_t isr_clear;
    uint32_t pending_irq;
    uint32_t pending_fiq;
  } set[4];

  uint32_t ilr[nirq];
};

static struct intc *intc = nil;

static void (*kernel_handlers[nirq])(size_t) = { nil };

  static void
mask_intr(uint32_t irqn)
{
  uint32_t mask, mfield;

  mfield = irqn / 32;
  mask = 1 << (irqn % 32);

  intc->set[mfield].mir_set = mask;
}

  static void
unmask_intr(uint32_t irqn)
{
  uint32_t mask, mfield;

  mfield = irqn / 32;
  mask = 1 << (irqn % 32);

  intc->set[mfield].mir_clear = mask;
}

  int
add_kernel_irq(size_t irqn, void (*func)(size_t))
{
  kernel_handlers[irqn] = func;

  unmask_intr(irqn);

  return OK;
}

int
add_user_irq(size_t irqn, proc_t p)
{
  return ERR;
}

  static void
irq_handler(void)
{
  uint32_t irq;

  irq = intc->sir_irq;

  debug("interrupt %i\n", irq);

  if (kernel_handlers[irq]) {
    kernel_handlers[irq](irq);

  } else {
    mask_intr(irq);
  }

  /* Allow new interrupts. */
  intc->control = 1;
}

  void
trap(size_t pc, int type)
{
  uint32_t fsr;
  size_t addr;

  debug("trap at 0x%h, type %i\n", pc, type);

  switch(type) {
    case ABORT_INTERRUPT:
      irq_handler();

      return; /* Note the return. */

    case ABORT_INSTRUCTION:
      debug("abort instruction at 0x%h\n", pc);
      break;

    case ABORT_PREFETCH:
      debug("prefetch instruction at 0x%h\n", pc);
      break;

    case ABORT_DATA:
      addr = fault_addr();
      fsr = fsr_status() & 0xf;

      debug("data abort at 0x%h for 0x%h type 0x%h\n", pc, addr, fsr);

      switch (fsr) {
	case 0x5: /* section translation */
	case 0x7: /* page translation */
	case 0x0: /* vector */
	case 0x1: /* alignment */
	case 0x3: /* also alignment */
	case 0x2: /* terminal */
	case 0x4: /* external linefetch section */
	case 0x6: /* external linefetch page */
	case 0x8: /* external non linefetch section */
	case 0xa: /* external non linefetch page */
	case 0x9: /* domain section */
	case 0xb: /* domain page */
	case 0xc: /* external translation l1 */
	case 0xe: /* external translation l2 */
	case 0xd: /* section permission */
	case 0xf: /* page permission */
	default:
	  break;
      }

      break;
  }

  if (up == nil) {
    panic("trap with no proc on cpu!!!\n");
  }

  debug("killing proc %i\n", up->pid);

  up->state = PROC_dead;

  schedule(nil);

  /* Never reached */
}

  void
init_intc(void)
{
  int i;

  if (intc == nil) {
    return;
  }

  intc->control = 1;

  /* enable interface auto idle */
  intc->sysconfig = 1;

  /* mask all interrupts. */
  for (i = 0; i < 4; i++) {
    intc->set[i].mir = 0xffffffff;
  }

  /* Set all interrupts to lowest priority. */
  for (i = 0; i < nirq; i++) {
    intc->ilr[i] = 63 << 2;
  }

  intc->control = 1;
}

  void
map_intc(void)
{
  size_t addr, size;

  addr = 0x48200000;
  size = 0x1000;

  intc = (struct intc *) kernel_va_slot;
  map_pages(kernel_l2, addr, kernel_va_slot, size, AP_RW_NO, false);
  kernel_va_slot += PAGE_ALIGN(size);
}

