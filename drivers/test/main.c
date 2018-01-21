#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>

void
main(void)
{
  uint8_t m[MESSAGE_LEN];
  int i, pid;
	uint32_t j;

	/* Pid of uart driver. */
	pid = 1;

  i = 0;
  while (true) {
    snprintf((char *) m, MESSAGE_LEN, "Hello. %i\n", i++);
    send(pid, m);
    yield();

		if (i % 10 == 0) {
			for (j = 0; j < 0xffffff; j++)
				;
		}
  }
}

