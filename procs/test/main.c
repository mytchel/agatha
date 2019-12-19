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
#include <fat.h>
#include <timer.h>
#include <log.h>
#include <crc.h>

#define TEST_MEM 0
#define TEST_TMR 0
#define TEST_FAT 0
#define TEST_NET 0

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
	log_init("test");

	log(LOG_INFO, "test starting");

	uint8_t t[] = "hello there";

	uint32_t crc = crc_checksum(t, sizeof(t)-1);

	log(LOG_INFO, "crc = 0x%x", crc);

#if TEST_TMR
	test_timer();
#endif

#if TEST_MEM
	test_mem();
#endif

#if TEST_FAT
	test_fat_fs();
#endif

#if TEST_NET
	test_net();
#endif

	exit(OK);
}

