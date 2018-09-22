#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <dev_reg.h>
#include <block_dev.h>

char *debug_name = "serial0";
int debug_pid;

char *block_name = "sdmmc0";
int block_pid;

void
debug(char *fmt, ...)
{
	uint8_t m[MESSAGE_LEN];
	va_list a;

	va_start(a, fmt);
	vsnprintf((char *) m, sizeof(m), fmt, a);
	va_end(a);

	send(debug_pid, m);
	recv(debug_pid, m);
}

int
get_device_pid(char *name)
{
	union dev_reg_req rq;
	union dev_reg_rsp rp;

	rq.find.type = DEV_REG_find;
	snprintf(rq.find.name, sizeof(rq.find.name),
			"%s", name);

	if (send(DEV_REG_PID, &rq) != OK) {
		return ERR;
	} else if (recv(DEV_REG_PID, &rp) != DEV_REG_PID) {
		return ERR;
	} else if (rp.find.ret != OK) {
		return ERR;
	}

	return rp.find.pid;
}

int
test_block(int bpid)
{
	union block_dev_req rq;
	union block_dev_rsp rp;
	size_t pa, len, i;
	char *a;
	int ret;

	len = PAGE_SIZE;

	debug("test_block\n");

	pa = request_memory(len);
	if (pa == nil) {
		debug("test_block request memory failed\n");
		return ERR;
	}
		
	debug("test_block using 0x%h, 0x%h\n", pa, len);

	debug("test_block give memory at block device\n");

	if ((ret = give_addr(bpid, pa, len)) != OK) {
		debug("test_block give addr failed %i\n", ret);
		return ERR;
	}

	debug("test_block sending request\n");

	rq.read.type = BLOCK_DEV_read;
	rq.read.pa = pa;
	rq.read.len = len;
	rq.read.start = 0;
	rq.read.nblocks = 8;
	
	if (send(bpid, &rq) != OK) {
		debug("test_block send failed\n");
		return ERR;
	} else if (recv(bpid, &rp) != bpid) {
		debug("test_block recv failed\n");
		return ERR;
	} else if (rp.read.ret != OK) {
		debug("test_block read returned bad %i\n", rp.read.ret);
		return ERR;
	}

	debug("test_block read responded good, map\n");

	a = map_addr(pa, len, MAP_RO);
	if (a == nil) {
		debug("test_block map failed\n");
		return ERR;
	}

	debug("test_block mapped at 0x%h\n", a);

	for (i = 0; i < len - 3; i += 4) {
		debug("%i: %h %h %h %h\n", i, a[i], a[i+1], a[i+2], a[i+3]);
	}

	debug("test_block unmap and release\n");

	unmap_addr(a, len);
	release_addr(pa, len);

	return OK;
}

	void
main(void)
{
	do {
		debug_pid = get_device_pid(debug_name);
	} while (debug_pid < 0);

	debug("init using %s for debug\n", debug_name);

	do {
		block_pid = get_device_pid(block_name);
	} while (block_pid < 0);

	debug("init using block device %s at %i\n", block_name, block_pid);

	if (test_block(block_pid) != OK) {
		raise();
	}
}

