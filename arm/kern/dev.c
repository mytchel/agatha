#include "../../kern/head.h"
#include "fns.h"
#include <fdt.h>

extern uint32_t *_kernel_start;
extern uint32_t *_kernel_end;

static kframe_t mem_frames = nil;

  size_t
get_ram(size_t size, size_t align)
{
  kframe_t *h, f, n;
  size_t pa;

  for (h = &mem_frames; *h != nil; h = &(*h)->next) {
    f = *h;

    pa = (f->u.pa + align - 1) & ~(align - 1);
    if (pa - f->u.pa + f->u.len >= size) {
      if (pa != f->u.pa) {
        /* split off the front and put in in the list. */
        n = frame_split(f, pa - f->u.pa);
        f = n;
      } else {
        *h = f->next;
      }

      if (f->u.len > size) {
        /* split of rest. */
        n = frame_split(f, size);
        n->next = mem_frames;
        mem_frames = n;
      }

      pa = f->u.pa;

      /* TODO: mark frame structure as unused. */

      return pa;
    }
  }

  panic("failed to get memory of size 0x%h, aligned to 0x%h\n",
      size, align);
}

void
split_out_mem(size_t start, size_t size)
{
  kframe_t *h, f, n;

  for (h = &mem_frames; *h != nil; h = &(*h)->next) {
    f = *h;

    if (f->u.pa <= start && start + size <= f->u.pa + f->u.len) {
      if (f->u.pa < start) {
        n = frame_split(f, start - f->u.pa);
        f = n;
      } else {
        *h = f->next;
      }

      if (f->u.len > size) {
        n = frame_split(f, size);
        n->next = mem_frames;
        mem_frames = n;
      }

      /* TODO: make frame structure as unused. */

      return;
    }
  }
}

void
give_remaining_ram(proc_t p)
{
  kframe_t f, fn;

  f = mem_frames;
  while (f != nil) {
    fn = f->next;

    debug("give p0 from 0x%h to 0x%h\n", f->u.pa, f->u.len);
    frame_add(p, f);

    f = fn;
  }
}

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
     rather than only supporting ti,am33xx-intc as is currenlty
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

  static bool
fdt_memory_cb(void *dtb, void *node)
{
  size_t addr, size;
  kframe_t f;

  if (!fdt_node_regs(dtb, node, 0, &addr, &size)) {
    panic("failed to get memory registers!\n");
  }

  debug("memory at 0x%h, 0x%h\n", addr, size);

  f = frame_new(addr, size, F_TYPE_MEM);
  if (f == nil) {
    panic("failed to get frame for memory!\n");
  }

  f->next = mem_frames;
  mem_frames = f;

  return true;
}

void
fdt_reserved_cb(size_t start, size_t len)
{
  split_out_mem(start, len);
}

  void
fdt_get_memory(void *dtb)
{
  fdt_find_node_device_type(dtb, "memory",
      &fdt_memory_cb);

  if (mem_frames == nil) {
    panic("no memory found!\n");
  }
  
  fdt_check_reserved(dtb, &fdt_reserved_cb);
}

