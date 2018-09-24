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

#include "fat.h"

char *debug_name = "serial0";
int debug_pid;

char *block_name = "sdmmc0";
int partition = 0;
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
read_blocks(struct fat *fat, size_t pa, size_t len,
		size_t start, size_t r_len)
{
	union block_dev_req rq;
	union block_dev_rsp rp;
	int ret;

	debug("give block pid %i from 0x%h 0x%h\n", fat->block_pid, pa, len);
	if ((ret = give_addr(fat->block_pid, pa, len)) != OK) {
		debug("block give addr failed %i\n", ret);
		return ERR;
	}

	debug("block sending request\n");

	rq.read.type = BLOCK_DEV_read;
	rq.read.pa = pa;
	rq.read.len = len;
	rq.read.start = fat->start * fat->block_size + start;
	rq.read.r_len = r_len;
	
	if (send(fat->block_pid, &rq) != OK) {
		debug("block send failed\n");
		return ERR;
	} else if (recv(fat->block_pid, &rp) != fat->block_pid) {
		debug("block recv failed\n");
		return ERR;
	} else if (rp.read.ret != OK) {
		debug("block read returned bad %i\n", rp.read.ret);
		return ERR;
	}

	debug("block read responded good, map\n");
	return OK;
}

int
read_mbr(struct fat *fat)
{
	union block_dev_req rq;
	union block_dev_rsp rp;
	struct mbr *mbr;
	size_t pa, len;
	int ret;

	debug("reading mbr from %i\n", fat->block_pid);

	rq.info.type = BLOCK_DEV_info;
	
	if (send(fat->block_pid, &rq) != OK) {
		debug("block info send failed\n");
		return ERR;
	} else if (recv(fat->block_pid, &rp) != fat->block_pid) {
		debug("block info recv failed\n");
		return ERR;
	} else if (rp.info.ret != OK) {
		debug("block info returned bad %i\n", rp.info.ret);
		return ERR;
	}

	fat->block_size = rp.info.block_len;

	len = PAGE_ALIGN(sizeof(struct mbr));
	pa = request_memory(len);
	if (pa == nil) {
		debug("read mbr memory request failed\n");
		return ERR;
	}
	
	ret = read_blocks(fat, pa, len, 0, 1);
	if (ret != OK) {
		return ret;
	}

	mbr = map_addr(pa, len, MAP_RO);
	if (mbr == nil) {
		return ERR;
	}

	int i;
	for (i = 0; i < 4; i++) {
		debug("partition %i\n", i);
		debug("status = 0x%h\n", mbr->parts[i].status);
		debug("type = 0x%h\n", mbr->parts[i].type);
		debug("lba = 0x%h\n", mbr->parts[i].lba);
		debug("len = 0x%h\n", mbr->parts[i].sectors);
	}

	fat->start = mbr->parts[partition].lba;
	fat->nblocks = mbr->parts[partition].sectors;

	debug("partition %i starts at 0x%h and goes for 0x%h blocks\n",
			partition, fat->start, fat->nblocks);

	unmap_addr(mbr, len);	
	release_addr(pa, len);

	return OK;
}

	void
main(void)
{
	struct fat fat;

	do {
		debug_pid = get_device_pid(debug_name);
	} while (debug_pid < 0);

	do {
		block_pid = get_device_pid(block_name);
	} while (block_pid < 0);


	fat.block_pid = block_pid;

	debug("fat fs starting on %s.%i\n", block_name, partition);

	debug("read mbr\n");
	if (read_mbr(&fat) != OK) {
		debug("fat_read_mbr failed\n");
		raise();
	}

	if (fat_read_bs(&fat) != OK) {
		debug("fat_read_bs failed\n");
		raise();
	}

	while (true) {
		yield();
	}
}


