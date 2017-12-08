#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>

int
main(void)
{
	struct frame f, l2 = { 0 };
	int i, count;
  size_t off;
  void *dtb;

  count = frame_count();
  for (i = 0; i < count && frame_info(i, &f) == OK; i++) {
    if ((f.flags & F_MAP_TYPE_MASK) == F_MAP_TYPE_TABLE_L2) {
      memcpy(&l2, &f, sizeof(struct frame));
    }
  } 

  if (l2.f_id == 0) {
    while (true)
        ;
  }

  ((uint32_t *) l2.va)[0] = 1234;

  off = PAGE_SIZE * 10;
  while (((uint32_t *) l2.va)[L2X(off)] != 0)
    off += PAGE_SIZE;
  
  for (i = 0; i < count && frame_info(i, &f) == OK; i++) {
    if (f.type == F_TYPE_FDT) {
      if (frame_map(l2.f_id, f.f_id, (void *) (l2.t_va + off), 
            F_MAP_TYPE_PAGE|F_MAP_READ) == OK) {

        dtb = (void *) (l2.t_va + off);
        off += f.len;
      } else {
        while (true)
           ;
      }
    }
  } 


  while (true)
    ;

  return OK;
}

