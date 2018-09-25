#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <dev_reg.h>
#include <block_dev.h>
#include <mbr.h>
#include <fs.h>

#include "fat.h"

	void
main(void)
{
	union file_req rq;
	union file_rsp rp;
	int from;
	
	while (true) {
		if ((from = recv(-1, &rq)) < 0)
			continue;

		if (rq.type == FILE_start) {
			rp.untyped.type = rq.type;
			rp.untyped.ret = OK;
			send(from, &rp);

			/* TODO: Fork */
			fat_mount(rq.start.dev_pid, rq.start.partition);
		} else {
			rp.untyped.type = rq.type;
			rp.untyped.ret = ERR;
			send(from, &rp);
		}
	}

	raise();
}


