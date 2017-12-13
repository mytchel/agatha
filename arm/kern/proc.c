#include "../../kern/head.h"
#include "fns.h"
#include "trap.h"

struct bundle_proc {
  uint8_t name[256];
  size_t len;
};

struct bundle_proc bundle[] = {
#include "../bundle.list"
};

extern uint32_t *_binary_bundle_bin_start;
extern uint32_t *_binary_bundle_bin_end;

void
func_label(label_t *l, 
           size_t stack,
           size_t stacklen,
           size_t func)
{
	memset(l, 0, sizeof(struct label));

	l->psr = MODE_SVC;
	l->sp = (uint32_t) stack + stacklen;
	l->pc = (uint32_t) func;
}

void
__attribute__((noreturn))
proc_start(void)
{
  uint8_t m[MESSAGE_LEN];
  uint32_t *r;
  label_t u = {0};

  debug("in proc_start for %i\n", up->pid);

  while (recv(m) != OK) {
    debug("%i proc_start: krecv errored!\n", up->pid);
  }

  r = (uint32_t *) m;
  u.pc = r[0];
  u.sp = r[1];

  debug("drop_to_user %i at 0x%h 0x%h\n", up->pid, u.pc, u.sp);
  drop_to_user(&u, up->kstack, KSTACK_LEN);
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

  debug("create proc for 0x%h, 0x%h\n", start, len);

  p = proc_new();
  if (p == nil) {
    panic("Failed to create proc!\n");

  }

  func_label(&p->label, (size_t) p->kstack, KSTACK_LEN, 
      (size_t) &bundle_proc_start);

  s = get_ram(0x5000, 0x4000);
  debug("l1 and l2 at 0x%h\n", s);

  /* Temporarily map pages for editing. */
  debug("temporarily map to 0x%h in kernel l2 at 0x%h\n", kernel_va_slot, l2);
  map_pages(l2, s, kernel_va_slot, 0x5000, AP_RW_NO, false);

  debug("copy ttb at 0x%h\n", ttb);

  memcpy((void *) kernel_va_slot, 
      ttb, 
      0x4000);

  debug("create new frame for l1\n");

  fl1 = frame_new(s, 0x4000, F_TYPE_MEM);
  frame_add(p, fl1);

  memset((void *) (kernel_va_slot + 0x4000), 0, 0x1000);

  debug("create new frame for l2\n");

  fl2 = frame_new(s + 0x4000, 0x1000, F_TYPE_MEM);
  frame_add(p, fl2);

  debug("setup l1 and l2 frame structures\n");

  fl1->u.flags = F_MAP_TYPE_TABLE_L1|F_MAP_READ;
  fl1->u.t_va = 0;
  fl1->u.t_id = fl1->u.f_id;

  fl2->u.flags = F_MAP_TYPE_TABLE_L2|F_MAP_READ;
  fl2->u.t_va = 0;
  fl2->u.t_id = fl1->u.f_id;

  /* Temporary mappings. */	
  fl1->u.va = kernel_va_slot;
  fl2->u.va = kernel_va_slot + 0x4000;

  debug("map l2 into l1\n");
  map_l2((void *) fl1->u.va, fl2->u.pa, 0);

  debug("map l1 into l2 as pages for userspace editing.\n");

  map_pages((void *) fl2->u.va, fl1->u.pa, 0x1000, 
      fl1->u.len, AP_RW_RO, true);

  debug("map l2 into l2 as pages for userspace editing.\n");

  map_pages((void *) fl2->u.va, fl2->u.pa, 0x1000 + fl1->u.len, 
      fl2->u.len, AP_RW_RO, true);

  p->vspace = fl1;

  s = start;
  l = len;
  debug("map user code from 0x%h to 0x%h\n", s, USER_ADDR);

  map_pages((void *) fl2->u.va, s, USER_ADDR, 
      l, AP_RW_RW, true);


/*
  f = frame_new(s, l, F_TYPE_MEM);
  frame_add(p, f);
  frame_map(fl2, f, USER_ADDR, 
      F_MAP_TYPE_PAGE|F_MAP_READ|F_MAP_WRITE);

  l = PAGE_SIZE * 2;
  s = get_ram(l, PAGE_SIZE);
  debug("map stack space from 0x%h to 0x%h\n", s, USER_ADDR - l);

  f = frame_new(s, l, F_TYPE_MEM);
  frame_add(p, f);
  frame_map(fl2, f, USER_ADDR - l,
      F_MAP_TYPE_PAGE|F_MAP_READ|F_MAP_WRITE);
*/

  debug("remove l1 and l2 from kernel l2\n");

  /* Remove the section mapping from the default address space. */
  unmap_pages(l2, kernel_va_slot, 0x5000);

  debug("fix l1 and l2 mappings\n");

  /* Fix mappings. */	
  fl1->u.va = 0x1000;
  fl2->u.va = 0x1000 + fl1->u.len;

  return p;
}

  proc_t
init_procs(void)
{
  proc_t p0, p;
  size_t off;
  int i;

  off = (size_t) &_binary_bundle_bin_start;
  i = 0;

  debug("create proc0\n");

  p0 = init_proc(off, bundle[i].len);
  if (p0 == nil) {
    panic("Failed to create p0!\n");
  }

  off += bundle[i].len;
  i++;

  /* Create remaining procs. */

  while(i < sizeof(bundle)/sizeof(bundle[0])) {
    debug("create proc %s\n", bundle[i].name);
    p = init_proc(off, bundle[i].len);
    if (p == nil) {
      panic("Failed to create proc %s!\n", bundle[i].name);
    }

    off += bundle[i].len;
    i++;
  }

  return p0;
}

