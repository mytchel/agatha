#include <types.h>
#include <err.h>
#include <mach.h>
#include <sys.h>
#include <sysobj.h>
#include <c.h>
#include <mesg.h>
#include <proc0.h>
#include <mmu.h>
#include <stdarg.h>
#include <string.h>
#include <block.h>
#include <mbr.h>
#include <timer.h>
#include <log.h>

	void
main(void)
{
	log_init("fs");

	log(LOG_INFO, "fs starting");

	exit(OK);
}

