#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>

int
main(void)
{
	struct frame f;
	int i, count;

  count = frame_count();
  for (i = 0; i < count && frame_info(i, &f) == OK; i++) {

  } 

  while (true)
    ;

  return OK;
}

