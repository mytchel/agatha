#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>

int
main(void)
{
	struct frame f;
	int i, count;

  count = frame_count();
  for (i = 0; i < count; i++) {
    if (frame_info(i, &f) != OK) {
      raise();
    }
  } 

  return OK;
}

