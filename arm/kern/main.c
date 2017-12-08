#include "../../kern/head.h"
#include "fns.h"
#include <fdt.h>

uint32_t
ttb[4096]__attribute__((__aligned__(16*1024))) = { 0 };

/* TODO: store this for each proc in bundle_proc list. */
#define USER_ADDR 0x100000

struct bundle_proc {
  uint8_t name[256];
  size_t len;
};

struct bundle_proc bundle[] = {
#include "../bundle.list"
};

extern uint32_t *_kernel_start;
extern uint32_t *_kernel_end;
extern uint32_t *_binary_bundle_bin_start;
extern uint32_t *_binary_bundle_bin_end;

static kframe_t mem_frames = nil;

static uint32_t *kernel_l2;
static size_t kernel_va_slot;

  static size_t
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

  static void
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

  static void
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

  static void
bundle_proc_start(void)
{
  label_t u = {0};

  u.sp = (size_t) USER_ADDR;
  u.pc = (size_t) USER_ADDR;

  debug("proc %i drop to user at 0x%h 0x%h!\n", up->pid, u.pc, u.sp);
  drop_to_user(&u, up->kstack, KSTACK_LEN);
}

  static proc_t
init_proc(size_t start, size_t len)
{
  kframe_t f, fl1, fl2;
  size_t s, l;
  proc_t p;

  p = proc_new();
  if (p == nil) {
    panic("Failed to create proc!\n");
  }

  func_label(&p->label, (size_t) p->kstack, KSTACK_LEN, 
      (size_t) &bundle_proc_start);

  s = get_ram(0x5000, 0x4000);
  map_pages(kernel_l2, s, kernel_va_slot, 0x5000, AP_RW_NO, false);

  memcpy((void *) kernel_va_slot, 
      ttb, 
      0x4000);

  fl1 = frame_new(s, 0x4000, F_TYPE_MEM);
  frame_add(p, fl1);

  memset((void *) (kernel_va_slot + 0x4000), 0, 0x1000);

  fl2 = frame_new(s + 0x4000, 0x1000, F_TYPE_MEM);
  frame_add(p, fl2);

  map_l2((void *) kernel_va_slot, s + 0x4000, 0);
  map_pages((void *) (kernel_va_slot + 0x4000), s, 0x1000, 0x5000, AP_RW_RO, true);

  p->vspace = fl1;

  fl1->u.flags = F_MAP_TYPE_TABLE_L1|F_MAP_READ;
  fl1->u.t_va = 0;
  fl1->u.t_id = fl1->u.f_id;

  fl2->u.flags = F_MAP_TYPE_TABLE_L2|F_MAP_READ;
  fl2->u.t_va = 0;
  fl2->u.t_id = fl1->u.f_id;

  /* Temporary mappings. */	
  fl1->u.va = kernel_va_slot;
  fl2->u.va = kernel_va_slot + 0x4000;

  s = start;
  l = len;
  f = frame_new(s, l, F_TYPE_MEM);
  frame_add(p, f);
  frame_map(fl2, f, USER_ADDR, 
      F_MAP_TYPE_PAGE|F_MAP_READ|F_MAP_WRITE);

  l = PAGE_SIZE * 4;
  s = get_ram(l, PAGE_SIZE);
  f = frame_new(s, l, F_TYPE_MEM);
  frame_add(p, f);
  frame_map(fl2, f, USER_ADDR - l,
      F_MAP_TYPE_PAGE|F_MAP_READ|F_MAP_WRITE);

  /* Remove the section mapping from the default address space. */
  unmap_pages(kernel_l2, kernel_va_slot, 0x5000);

  /* Fix mappings. */	
  fl1->u.va = 0x1000;
  fl2->u.va = 0x1000 + 0x4000;

  return p;
}

  static proc_t
init_procs(size_t dtb, size_t dtb_len)
{
  proc_t p0, p;
  kframe_t f;
  size_t off;
  int i;

  off = (size_t) &_binary_bundle_bin_start;
  i = 0;

  p0 = init_proc(off, bundle[i].len);
  if (p0 == nil) {
    panic("Failed to create p0!\n");
  }

  f = frame_new(dtb, dtb_len, F_TYPE_FDT);
  frame_add(p0, f);

  off += bundle[i].len;
  i++;

  /* Create remaining procs. */

  while(i < sizeof(bundle)/sizeof(bundle[0])) {
    p = init_proc(off, bundle[i].len);
    if (p == nil) {
      panic("Failed to create proc %s!\n", bundle[i].name);
    }

    off += bundle[i].len;
    i++;
  }

  give_remaining_ram(p0);

  return p0;
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
  map_pages(kernel_l2, addr, kernel_va_slot, size, AP_RW_NO, true);
  kernel_va_slot += size;
}

  void
map_devs(void *dtb)
{
  fdt_get_intc(dtb);

  /* Hardcode uart for now. */
  uart_regs = (void *) kernel_va_slot;
  map_pages(kernel_l2, 0x44e09000, kernel_va_slot, 0x2000, AP_RW_NO, true);
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
fdt_get_memory(void *dtb)
{
  fdt_find_node_device_type(dtb, "memory",
      &fdt_memory_cb);

  if (mem_frames == nil) {
    panic("no memory found!\n");
  }
}

void
fdt_reserved_cb(size_t start, size_t len)
{
  split_out_mem(start, len);
}

void
fdt_remove_reserved(void *dtb)
{
  fdt_check_reserved(dtb, &fdt_reserved_cb);
}

  void
main(size_t kernel_start, 
    size_t dtb, 
    size_t dtb_len)
{
  size_t kernel_len;
  size_t s, l;
  proc_t p;

  /* Early uart. */
  init_uart((void *) 0x44E09000);

  debug("kernel_start = 0x%h\n", kernel_start);

  fdt_get_memory((void *) dtb);

  kernel_len = (size_t) &_kernel_end - (size_t) &_kernel_start;
  split_out_mem(kernel_start, kernel_len);

  fdt_remove_reserved((void *) dtb);

  /* TODO: remove kernel and dtb from mem_frames. */

  s = get_ram(PAGE_SIZE, PAGE_SIZE);
  kernel_l2 = (uint32_t *) s;
  memset(kernel_l2, 0, PAGE_SIZE);

  map_l2(ttb, s, kernel_start);

  kernel_va_slot = kernel_start;

  s = kernel_start;
  l = PAGE_ALIGN((size_t) &_kernel_end - kernel_start);

  map_pages(kernel_l2, s, kernel_va_slot, l, AP_RW_NO, true);

  kernel_va_slot += l;

  map_devs((void *) dtb);

  debug("load and enable mmu\n");

  mmu_load_ttb(ttb);
  mmu_enable();

  init_devs();

  debug("init procs\n");

  p = init_procs(dtb, dtb_len);

  debug("start!\n");

  schedule(p);
}


