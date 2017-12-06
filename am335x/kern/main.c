#include "head.h"
#include "fns.h"
#include <fdt.h>

uint32_t
ttb[4096]__attribute__((__aligned__(16*1024))) = { 0 };

struct bundled_proc {
  uint8_t name[256];
  size_t len;
};

struct bundled_proc bundled[] = {
#include "../bundled.list"
};

extern uint32_t *_ram_start;
extern uint32_t *_ram_end;
extern uint32_t *_kernel_start;
extern uint32_t *_kernel_end;
extern uint32_t *_ex_stack_top;
extern uint32_t *_binary_bundled_bin_start;
extern uint32_t *_binary_bundled_bin_end;

static size_t ram_p = (size_t) &_ram_start;

  static size_t
get_ram(size_t l)
{
  size_t r;

  /* TODO: make this skip kernel, dtb, whatever else needs
     to be kept. */

  l = PAGE_ALIGN(l);

  r = ram_p;
  ram_p += l;

  return r;
}

  static void
give_remaining_ram(proc_t p)
{
  size_t s, l;
  kframe_t f;

  s = ram_p;
  l = (size_t) &_kernel_start - s;
  debug("give p0 from 0x%h to 0x%h\n", s, s + l);
  f = frame_new(s, l, F_TYPE_MEM);
  frame_add(p, f);

  s = (size_t) PAGE_ALIGN(&_kernel_end);
  l = (size_t) &_ram_end - s;
  debug("give p0 from 0x%h to 0x%h\n", s, s + l);
  frame_new(s, l, F_TYPE_MEM);	
  frame_add(p, f);
}

  static void
bundled_proc_start(void)
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
  size_t s, l, pa, va;
  kframe_t f, fl1, fl2;
  proc_t p;

  p = proc_new();
  if (p == nil) {
    panic("Failed to create proc!\n");
  }

  func_label(&p->label, (size_t) p->kstack, KSTACK_LEN, 
      (size_t) &bundled_proc_start);

  /* Oversized to get alignement right. */
  l = 0x9000;
  s = get_ram(l);
  if (!s) 
    panic("Failed to get mem for bundled proc's address space!\n");

  map_sections(ttb, SECTION_ALIGN_DN(s), KERNEL_ADDR - SECTION_SIZE,
      SECTION_SIZE, AP_RW_NO, false);

  pa = (s + 0x4000 - 1) & ~(0x4000 - 1);
  va = KERNEL_ADDR - SECTION_SIZE + pa - SECTION_ALIGN_DN(s);

  memcpy((void *) va, 
      ttb, 
      0x4000);

  /* Remove the section mapping from the procs address space. */
  unmap_sections((void *) va, KERNEL_ADDR - SECTION_SIZE, SECTION_SIZE);

  fl1 = frame_new(pa, 0x4000, F_TYPE_MEM);
  frame_add(p, fl1);

  memset((void *) (va + 0x4000), 0, 0x1000);

  fl2 = frame_new(pa + 0x4000, 0x1000, F_TYPE_MEM);
  frame_add(p, fl2);

  map_l2((void *) va, pa + 0x4000, 0);
  map_pages((void *) (va + 0x4000), pa, 0x1000, 0x5000, AP_RW_RO, true);

  p->vspace = fl1;

  fl1->u.flags = F_MAP_TYPE_TABLE_L1|F_MAP_READ;
  fl1->u.t_va = 0;
  fl1->u.t_id = fl1->u.f_id;

  fl2->u.flags = F_MAP_TYPE_TABLE_L2|F_MAP_READ;
  fl2->u.t_va = 0;
  fl2->u.t_id = fl1->u.f_id;

  /* Temporary mappings. */	
  fl1->u.va = va;
  fl2->u.va = va + 0x4000;

  s = start;
  l = len;
  f = frame_new(s, l, F_TYPE_MEM);
  frame_add(p, f);
  frame_map(fl2, f, USER_ADDR, 
      F_MAP_TYPE_PAGE|F_MAP_READ|F_MAP_WRITE);

  l = PAGE_SIZE * 4;
  s = get_ram(l);
  if (!s) 
    panic("Failed to get ram for stack!\n");

  f = frame_new(s, l, F_TYPE_MEM);
  frame_add(p, f);
  frame_map(fl2, f, USER_ADDR - l,
      F_MAP_TYPE_PAGE|F_MAP_READ|F_MAP_WRITE);

  /* Remove the section mapping from the default address space. */
  unmap_sections(ttb, KERNEL_ADDR - SECTION_SIZE, SECTION_SIZE);

  /* Fix mappings. */	
  fl1->u.va = 0x1000;
  fl2->u.va = 0x1000 + 0x4000;

  return p;
}

static proc_t
init_procs(void)
{
  proc_t p0, p;
  size_t off;
  int i;

  off = (size_t) &_binary_bundled_bin_start;
  i = 0;

  p0 = init_proc(off, bundled[i].len);
  if (p0 == nil) {
    panic("Failed to create proc0!\n");
  }

  off += bundled[i].len;
  i++;

  /* Create remaining procs. */

  while(i < sizeof(bundled)/sizeof(bundled[0])) {
    p = init_proc(off, bundled[i].len);
    if (p == nil) {
      panic("Failed to create proc %s!\n", bundled[i].name);
    }

    off += bundled[i].len;
    i++;
  }

  give_remaining_ram(p0);

  return p0;
}

static void *uart_regs, *intc_regs, *wd_regs, *t_regs, *tc_regs;

  size_t
map_devs(uint32_t *l2, size_t va)
{
  size_t l;

  /* UART0 */
  l = 0x1000;
  map_pages(l2, 0x44E09000, va, l, AP_RW_NO, false);
  uart_regs = (void *) va;
  va += l;

  /* INTCPS */
  l = 0x1000;
  map_pages(l2, 0x48200000, va, l, AP_RW_NO, false);
  intc_regs = (void *) va;
  va += l;

  /* Watchdog */
  l = 0x1000;
  map_pages(l2, 0x44E35000, va, l, AP_RW_NO, false);
  wd_regs = (void *) va;
  va += l;

  /* DMTIMER2 for systick. */
  l = 0x1000;
  map_pages(l2, 0x48040000, va, l, AP_RW_NO, false);
  t_regs = (void *) va;
  va += l;

  map_pages(l2, 0x44E00000, va, l, AP_RW_NO, false);
  tc_regs = (void *) (va + 0x500);
  va += l;

  return va;
}

  void
init_devs(void)
{
  init_uart(uart_regs);
  init_intc(intc_regs);
  init_watchdog(wd_regs);
  init_timers(t_regs, 68, tc_regs);
}

#define big_to_little16(X) \
  ((((X)>>8) & 0x00ff) | (((X)<<8) & 0xff00))

#define big_to_little32(X) \
    (big_to_little16((X)>>16) | \
     (big_to_little16((X) & 0xffff) << 16))

#define big_to_little64(X) \
    (big_to_little32((X)>>32) | \
     (big_to_little32((X) & 0xffffffff) << 32))

void
fdt_process_reserved_entries(struct fdt_reserve_entry *e)
{
  while (e->address != 0 && e->size != 0) {
    debug("have reserved entry for 0x%h 0x%h\n", 
        (uint32_t) big_to_little64(e->address), 
        (uint32_t) big_to_little64(e->size));
    e++;
  }
}

uint32_t *
fdt_process_node(void *dtb, 
    struct fdt_header *head, 
    uint32_t *pr,
    char *name,
    size_t oname, 
    size_t lname)
{
  struct fdt_prop prop;
  uint32_t p;
  char *pc;
  size_t i, l;

  pc = (char *) pr;
  for (i = 0; oname + 1 + i < lname && pc[i] != 0; i++)
    name[oname + 1 + i] = pc[i];

  name[oname] = '/';
  name[oname + 1 + i] = 0;

  debug("node '%s'\n", name);

  l = ((i+1) + 3) & ~3;
  pr = (uint32_t *) ((size_t) pr + l);

  while (true) {
    p = *pr++;
    p = big_to_little32(p);

    switch (p) {
      default:  
        return nil;

      case FDT_END_NODE:
        return pr;

      case FDT_BEGIN_NODE:
        pr = fdt_process_node(dtb, head, pr, 
            name, oname + i, lname);

        if (pr == nil) {
          return nil;
        } else {
          break;
        }

      case FDT_NOP:
        break;

      case FDT_PROP:
        p = *pr++;
        prop.len = big_to_little32(p);

        p = *pr++;
        prop.nameoff = big_to_little32(p);

        debug("prop 0x%h '%s'\n", prop.len, (char *) ((size_t) dtb + head->off_dt_strings + prop.nameoff));

        l = ((prop.len) + 3) & ~3;
        pr = (uint32_t *) ((size_t) pr + l);

        break;
    }
  }

  return pr;
}

int
fdt_check(void *dtb, struct fdt_header *head)
{
  uint32_t *pr, *ph;
  size_t i;

  pr = (uint32_t *) dtb;
  ph = (uint32_t *) head;
  for (i = 0; 
      i < sizeof(struct fdt_header) / sizeof(uint32_t); 
      i++) {

    ph[i] = big_to_little32(pr[i]);
  }

  if (head->magic != FDT_MAGIC) {
    debug("fdt magic 0x%h != constant 0x%h\n", 
        head->magic, FDT_MAGIC);

    return ERR;

  } else if (head->version != 17) {
    debug("fdt version %i is not 17 and so is not supported!\n",
        head->version);

    return ERR;

  } else {
    return OK;
  }
}

  void
fdt_process(void *dtb)
{
  struct fdt_header head;
  char name[256];

  if (fdt_check(dtb, &head) != OK) {
    panic("error processing fdt!\n");
  }

  fdt_process_reserved_entries((struct fdt_reserve_entry *) 
      ((size_t) dtb + head.off_mem_rsvmap));

  fdt_process_node(dtb, &head, 
      (uint32_t *) ((size_t) dtb + head.off_dt_struct + sizeof(uint32_t)),
      name, 0, sizeof(name));
}

  void
kmain(size_t kernel_start, 
    size_t dtb, 
    size_t dtb_len)
{
  uint32_t *l2;
  size_t s, l;
  proc_t p;

  /* Early uart. */
  init_uart((void *) 0x44E09000);

  debug("kernel_start = 0x%h\n", kernel_start);

  fdt_process((void *) dtb);

  s = get_ram(PAGE_SIZE);
  l2 = (uint32_t *) s;
  memset(l2, 0, PAGE_SIZE);

  map_l2(ttb, s, KERNEL_ADDR);

  s = kernel_start;
  l = PAGE_ALIGN((size_t) &_kernel_end - (size_t) &_kernel_start);

  map_pages(l2, s, KERNEL_ADDR, l, AP_RW_NO, true);

  map_devs(l2, KERNEL_ADDR + l);

  debug("load and enable mmu\n");

  mmu_load_ttb(ttb);
  mmu_enable();

  init_devs();

  debug("init procs\n");

  p = init_procs();

  debug("start!\n");

  schedule(p);
}


