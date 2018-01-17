#include "../../kern/head.h"
#include "fns.h"
#include "trap.h"

struct kernel_info
kernel_info __attribute__((__aligned__(0x1000))) = { 0 };

uint32_t
kernel_ttb[4096]__attribute__((__aligned__(0x4000))) = { 0 };

uint32_t
kernel_l2[1024]__attribute__((__aligned__(0x1000))) = { 0 };

static uint32_t
proc0_l1[4096]__attribute__((__aligned__(0x4000)));

static uint32_t
proc0_l2[1024]__attribute__((__aligned__(0x1000))) = { 0 };

static uint32_t
proc0_stack[4096]__attribute__((__aligned__(0x1000))) = { 0 };

extern uint32_t *_binary_arm_proc0_proc0_bin_start;
extern uint32_t *_binary_arm_proc0_proc0_bin_end;

extern uint32_t *_kernel_start;
extern uint32_t *_kernel_end;

size_t kernel_va_slot;
size_t proc0_kernel_info_va;

  static void
proc0_start(void)
{
  label_t u = {0};

  u.psr = MODE_USR;
  u.sp = USER_ADDR;
  u.pc = USER_ADDR;
  u.regs[0] = proc0_kernel_info_va;

  debug("proc %i drop to user at 0x%h 0x%h kernel at 0x%h!\n", 
      up->pid, u.pc, u.sp, up->kstack + KSTACK_LEN);

  drop_to_user(&u, up->kstack, KSTACK_LEN);
}

  static proc_t
init_proc0(void)
{
  kframe_t f, fl1, fl2;
  size_t s, l;
  proc_t p;

  debug("create proc 0\n");

  p = proc_new();
  if (p == nil) {
    panic("Failed to create proc!\n");
  }

  func_label(&p->label, (size_t) p->kstack, KSTACK_LEN, 
      (size_t) &proc0_start);

  memcpy(proc0_l1,
      kernel_ttb, 
      0x4000);

  fl1 = frame_new((size_t) proc0_l1, sizeof(proc0_l1), F_TYPE_MEM);
  frame_add(p, fl1);

  fl2 = frame_new((size_t) proc0_l2, sizeof(proc0_l2), F_TYPE_MEM);
  frame_add(p, fl2);

  fl1->u.t_flags = F_TABLE_L1|F_TABLE_MAPPED;
  fl1->u.v_flags = F_MAP_TYPE_TABLE_L1|F_MAP_READ;
  fl1->u.t_va = 0;
  fl1->u.v_id = fl1->u.f_id;

  fl2->u.t_flags = F_TABLE_L2|F_TABLE_MAPPED;
  fl2->u.v_flags = F_MAP_TYPE_TABLE_L2|F_MAP_READ;
  fl2->u.t_va = 0;
  fl2->u.v_id = fl1->u.f_id;
 
  /* Temporary va->pa mappings. */ 
  fl1->u.v_va = fl1->u.pa;
  fl2->u.v_va = fl2->u.pa;

  s = (size_t) &kernel_info;
  l = PAGE_ALIGN(sizeof(kernel_info));

  proc0_kernel_info_va = 
      0x1000 + fl1->u.len + fl2->u.len, 

  f = frame_new(s, l, F_TYPE_MEM);
  frame_add(p, f);
  frame_map(fl2, f, proc0_kernel_info_va, 
      F_MAP_TYPE_PAGE|F_MAP_READ);

  s = (size_t) &_binary_arm_proc0_proc0_bin_start;
  l = PAGE_ALIGN(&_binary_arm_proc0_proc0_bin_end)
    - s;

  f = frame_new(s, l, F_TYPE_MEM);
  frame_add(p, f);
  frame_map(fl2, f, USER_ADDR, 
      F_MAP_TYPE_PAGE|F_MAP_READ|F_MAP_WRITE);

  s = (size_t) proc0_stack;
  l = sizeof(proc0_stack);

  f = frame_new(s, l, F_TYPE_MEM);
  frame_add(p, f);
  frame_map(fl2, f, USER_ADDR - l,
      F_MAP_TYPE_PAGE|F_MAP_READ|F_MAP_WRITE);

  map_l2(proc0_l1, (size_t) proc0_l2, 0);

  map_pages(proc0_l2, fl1->u.pa, 0x1000, 
      fl1->u.len, AP_RW_RO, true);

  map_pages(proc0_l2, fl2->u.pa, 
      0x1000 + fl1->u.len, 
      fl2->u.len, AP_RW_RO, true);

  fl1->u.v_va = 0x1000;
  fl2->u.v_va = 0x1000 + fl1->u.len;
  
  p->vspace = fl1;

  return p;
}

  void
main(size_t kernel_start, 
    size_t dtb, 
    size_t dtb_len)
{
  size_t kernel_len;
  proc_t p0;

  kernel_len = PAGE_ALIGN(&_kernel_end) - kernel_start;

  kernel_info.kernel_start = kernel_start;
  kernel_info.kernel_len = kernel_len;

  kernel_info.dtb_start = dtb;
  kernel_info.dtb_len = dtb_len;

  map_l2(kernel_ttb, (size_t) kernel_l2, kernel_start);

  kernel_va_slot = kernel_start;

  map_pages(kernel_l2, kernel_start, kernel_va_slot, 
      kernel_len, AP_RW_NO, true);

  kernel_va_slot += kernel_len;

  map_devs((void *) dtb);

  mmu_load_ttb(kernel_ttb);

  init_devs();

  p0 = init_proc0();

  debug("start!\n");

  schedule(p0);
}


