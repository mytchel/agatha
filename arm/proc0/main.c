#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <fdt.h>

size_t
find_free(struct frame *f_l2, size_t len)
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
  
 return true;
}

void
main(struct kernel_info *kinfo)
{
  bool f_l1 = false, f_l2 = false;
	struct frame f, l1, l2;
	int i, count, f_id;
  void *va;

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
    return;
  }

  f_id = frame_create(kinfo->dtb_start, kinfo->dtb_len, F_TYPE_MEM);
  if (f_id < 0) {
    return;
  }
  
  va = (void *) find_free(&l2, kinfo->dtb_len); 

  if (frame_map(l2.f_id, f_id, va, F_MAP_TYPE_PAGE|F_MAP_READ) != OK)
    return;

  fdt_find_node_device_type(va, "memory", &memory_cb);

  while (true)
    ;
}

