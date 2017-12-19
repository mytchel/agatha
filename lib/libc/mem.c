#include <types.h>
#include <err.h>
#include <sys.h>
#include <mach.h>
#include <c.h>

/* TODO: this is machine depentant and should go under
   the machines directory. */

static int f_l1_id = -1, f_l2_id = -1;

static bool
find_tables(void)
{
  struct frame f;
  int i, count;

  count = frame_count();
  for (i = 0; i < count && frame_info_index(&f, i) == OK; i++) {
    switch (f.t_flags & F_TABLE_TYPE_MASK) {
      case F_TABLE_L1:
        f_l1_id = f.f_id;
        break;
      case F_TABLE_L2:
        f_l2_id = f.f_id;
        break;
      default:
        break;
    }
  }

  if (f_l1_id != -1 && f_l2_id != -1) {
    return true;
  } else {
    return false;
  }
}

static size_t
find_free_va(size_t len)
{
  struct frame f_l2;
  uint32_t *l2;
  size_t a;
  int i, j;

  if (frame_info(&f_l2, f_l2_id) != OK) {
    return nil;
  }

  l2 = (uint32_t *) f_l2.v_va;
  for (i = 0; i < f_l2.len / 4; i++) {
  too_small:
    if (l2[i] != 0 || (i == 0 && f_l2.t_va == 0)) 
      continue;

    a = i * PAGE_SIZE + f_l2.t_va;

    for (j = 1; j * PAGE_SIZE < len; j++) {
      if (l2[i+j] != 0) {
        i += j;
        goto too_small;
      }
    }
      
    return a;
  }
    
  return nil;
}

void *
frame_map_free(int f_id, int flags)
{
  struct frame f;
  size_t va;

  if (f_l1_id == -1 || f_l2_id == -1) {
    if (!find_tables()) {
      return nil;
    }
  } 

  if (frame_info(&f, f_id) != OK) {
    return nil;
  }

  va = find_free_va(f.len);
  if (va == nil) {
    return nil;
  }

  if (frame_map(f_l2_id, f_id, (void *) va, flags) != OK) {
    return nil;
  }

  return (void *) va;
}

