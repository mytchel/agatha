#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <fdt.h>

#include "proc0.h"

static int mf_free_i = 0, mf_used_i = 0;
static int mf_free[256] = { -1 };
static int mf_used[256] = { -1 };

static struct frame l1, l2;

static size_t
find_free_va(struct frame *f_l2, size_t len)
{
  uint32_t *l2;
  size_t a;
  int i, j;

  l2 = (uint32_t *) f_l2->va;
  for (i = 0; i < 0x1000; i++) {
too_small:
    if (l2[i] == 0) {
      a = i * PAGE_SIZE + f_l2->t_va;

      if (a == 0) continue;
      for (j = 1; j * PAGE_SIZE < len; j++) {
        if (l2[i+j] != 0) {
          i += j;
          goto too_small;
        }
      }

      return a;
    }
  }

  return nil;
}

void *
map_frame(int f_id, int flags)
{
  struct frame f;
  void *va;

  if (frame_info(&f, f_id) != OK) {
    return nil;
  }

  va = (void *) find_free_va(&l2, f.len);
  if (va == nil) {
    return nil;
  }

  if (frame_map(l2.f_id, f_id, va, flags) != OK) {
    return nil;
  }

  return va;
}

int
get_mem_frame(size_t len, size_t align)
{
 return nil; 
}

  static bool
memory_cb(void *dtb, void *node)
{
  size_t start, len;
  int f_id;

  if (!fdt_node_regs(dtb, node, 0, &start, &len)) {
    return false;
  }

  f_id = frame_create(start, len, F_TYPE_MEM);
  if (f_id < 0) {
    return false;
  }

  mf_free[mf_free_i++] = f_id;

  return true;
}

  static void
reserved_cb(size_t start, size_t len)
{
  struct frame f;
  int i, n, id;

  for (i = 0; i < mf_free_i; i++) {
    id = mf_free[i];

    if (frame_info(&f, id) != OK)
      continue;

    if (f.pa <= start && start + len < f.pa + f.len) {
      if (f.pa < start) {
        id = frame_split(id, start - f.pa);
        if (id < 0) 
          return;

        n = frame_split(id, len);
      } else {
        n = frame_split(id, len);
        mf_free[i] = n;
      }

      mf_free[mf_free_i++] = n;
      mf_used[mf_used_i++] = id;
      
      return;
    }
  }
}

bool
setup_mem(void)
{
  fdt_find_node_device_type(dtb, "memory", &memory_cb);
  
  fdt_check_reserved(dtb, &reserved_cb);

  reserved_cb(k_info->kernel_start, 
      k_info->kernel_len);

  return true;
}

bool
find_tables(void)
{
  bool f_l1 = false, f_l2 = false;
  struct frame f;
  int i, count;

  count = frame_count();
  for (i = 0; i < count && frame_info_index(&f, i) == OK; i++) {
    switch (f.flags & F_MAP_TYPE_MASK) {
      case F_MAP_TYPE_TABLE_L1:
        memcpy(&l1, &f, sizeof(struct frame));
        f_l1 = true;
        break;

      case F_MAP_TYPE_TABLE_L2:
        memcpy(&l2, &f, sizeof(struct frame));
        f_l2 = true;
        break;

      default:
        break;
    }
  } 

  if (!f_l1 || !f_l2) {
    return false;
  }

  return true;
}


