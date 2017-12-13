#include "../../kern/head.h"
#include "fns.h"
#include <fdt.h>

static void *uart_regs, *intc_regs;

void
fdt_get_intc(void *dtb)
{
  uint32_t intc_handle;
  size_t addr, size;
  void *node, *root;
  int len, l, i;
  char *data;

  root = fdt_root_node(dtb);
  if (root == nil) {
    panic("failed to find root node in fdt!\n");
  }

  len = fdt_node_property(dtb, root, "interrupt-parent", &data);
  if (len != sizeof(intc_handle)) {
    panic("failed to get interrupt-parent from root node!\n");
  }

  intc_handle = beto32(((uint32_t *) data)[0]);

  debug("interrupt controller has phandle 0x%h\n", intc_handle);

  node = fdt_find_node_phandle(dtb, intc_handle);
  if (node == nil) {
    panic("failed to find interrupt controller!\n");
  } 

  debug("interrupt controller %s\n", fdt_node_name(dtb, node));

  /* TODO: find compatable interrupt controller and use that
     rather than only supporting ti,am33xx-intc as is currently
     done. */

  len = fdt_node_property(dtb, node, "compatible", &data);
  if (len <= 0) {
    panic("failed to get interrupt controller compatbility list!\n");
  }

  for (i = 0; i < len; i += l + 1) {
    l = strlen(&data[i]);
    debug("have compatable string %i: '%s'\n", l, &data[i]);
  }

  if (!fdt_node_regs(dtb, node, 0, &addr, &size)) {
    panic("failed to get reg property for interupt controller!\n");
  }

  debug("intc regs at 0x%h to 0x%h\n", addr, size);

  intc_regs = (void *) kernel_va_slot;
  map_pages(l2, addr, kernel_va_slot, size, AP_RW_NO, true);
  kernel_va_slot += size;
}

  void
map_devs(void *dtb)
{
  fdt_get_intc(dtb);

  /* Hardcode uart for now. */
  uart_regs = (void *) kernel_va_slot;
  map_pages(l2, 0x44e09000, kernel_va_slot, 0x2000, AP_RW_NO, true);
  kernel_va_slot += 0x2000;
}

  void
init_devs(void)
{
  init_uart(uart_regs);
  init_intc(intc_regs);
}

