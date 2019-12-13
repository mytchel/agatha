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
#include <fs.h>
#include <fat.h>
#include <timer.h>
#include <log.h>

void
test_fat_fs(void);

void
test_timer(void);

void
test_mem(void);

void
test_net(void);

	void
main(void)
{
	log_init("init");

	log(LOG_INFO, "init starting");

	/*test_mem();*/

	/*test_fat_fs();*/

	test_net();
		
	/*test_timer();*/

	exit(OK);
}

