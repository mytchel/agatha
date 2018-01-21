#include "../../../kern/head.h"
#include "../fns.h"
#include "../trap.h"
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

static void (*handlers[nirq])(size_t) = {nil};

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

static int
am33xx_add_kernel_irq(size_t irqn, void (*func)(size_t))
{
  handlers[irqn] = func;
  unmask_intr(irqn);

	return OK;
}

static int
am33xx_add_user_irq(size_t irqn, proc_t p)
{
	if (handlers[irqn] != nil) {
		return ERR;
	}

  unmask_intr(irqn);

  return OK;
}

static void
irq_handler(void)
{
  uint32_t irq;
	
  irq = intc->sir_irq;
  
  if (handlers[irq]) {
    handlers[irq](irq);
  } else {
    mask_intr(irq);
		/* TODO: send message to registered interrupt. */
  }
}

static void
am33xx_trap(size_t pc, int type)
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
map_ti_am33xx_intc(void *dtb)
{
  uint32_t intc_handle;
  size_t addr, size;
  void *root, *node;
  int len, l, i;
  char *data;

	if (kernel_devices.trap != nil) {
		return;
	}

  root = fdt_root_node(dtb);
  if (root == nil) {
    return;
  }

  len = fdt_node_property(dtb, root, "interrupt-parent", &data);
  if (len != sizeof(intc_handle)) {
    return;
  }

  intc_handle = beto32(((uint32_t *) data)[0]);

  node = fdt_find_node_phandle(dtb, intc_handle);
  if (node == nil) {
    return;
  } 

  len = fdt_node_property(dtb, node, "compatible", &data);
  if (len <= 0) {
    return;
  }

  for (i = 0; i < len; i += l + 1) {
    l = strlen(&data[i]);
    if (strcmp(&data[i], "ti,am33xx-intc")) {
      goto good;
    }
  }

  return;
good:

  if (!fdt_node_regs(dtb, node, 0, &addr, &size)) {
    return;
  }

  intc = (struct intc *) kernel_va_slot;
  map_pages(kernel_l2, addr, kernel_va_slot, size, AP_RW_NO, false);
  kernel_va_slot += PAGE_ALIGN(size);

	kernel_devices.trap = &am33xx_trap;
	kernel_devices.add_kernel_irq = &am33xx_add_kernel_irq;
	kernel_devices.add_user_irq = &am33xx_add_user_irq;
}

void
init_ti_am33xx_intc(void)
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


