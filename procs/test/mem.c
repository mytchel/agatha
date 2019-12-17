#include <types.h>
#include <err.h>
#include <mach.h>
#include <sys.h>
#include <sysobj.h>
#include <c.h>
#include <mesg.h>
#include <proc0.h>
#include <stdarg.h>
#include <string.h>
#include <log.h>

void
test_mem(void)
{
	uint32_t **p;

	p = malloc(10 * sizeof(uint32_t));
	if (p == nil) {
		log(LOG_FATAL, "error allocating pointer buffer");
		exit(3);
	}

	size_t i, j;
	for (i = 1; i < (1<<15); i *= 2) {
		for (j = 0; j < 10; j++) {
			p[j] = malloc(i + j);
			if (p[j] == nil) {
				log(LOG_FATAL, "error allocating buffer %i size 0x%x",
						j, i + j);
			} else {
				log(LOG_INFO, "buffer %i of size 0x%x at 0x%x", j, i + j, p[j]);
				memset(p[j], j, i + j);
			}
		}	

		for (j = 0; j < 10; j++) {
			log(LOG_INFO, "free buffer %i at 0x%x", j, p[j]);
			free(p[j]);
		}
	}

	log(LOG_INFO, "alloc test finished!");
}

