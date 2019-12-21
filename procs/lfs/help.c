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
#include <log.h>

int
block_read(int eid, int fid,
		size_t blk, size_t n)
{
	union block_req rq;
	union block_rsp rp;

	log(LOG_INFO, "block read 0x%x", blk);

	rq.read.type = BLOCK_read;
	rq.read.off = 0;
	rq.read.blk = blk;
	rq.read.n_blks = n;

	if (mesg_cap(eid, &rq, &rp, fid) != OK) {
		log(LOG_FATAL, "block send mesg failed");
		return ERR;
	} else if (rp.read.ret != OK) {
		log(LOG_FATAL, "block read response bad %i", rp.read.ret);
		return ERR;
	}

	return OK;
}

int
block_write(int eid, int fid,
		size_t blk, size_t n)
{
	union block_req rq;
	union block_rsp rp;

	log(LOG_INFO, "block write 0x%x", blk);

	rq.write.type = BLOCK_write;
	rq.write.off = 0;
	rq.write.blk = blk;
	rq.write.n_blks = n;

	if (mesg_cap(eid, &rq, &rp, fid) != OK) {
		log(LOG_FATAL, "block send mesg failed");
		return ERR;
	} else if (rp.write.ret != OK) {
		log(LOG_FATAL, "block write response bad %i", rp.write.ret);
		return ERR;
	}

	return OK;
}

int
block_get_block_size(int eid, size_t *blk_size)
{
	union block_req rq;
	union block_rsp rp;

	rq.info.type = BLOCK_info;
	
	if (mesg(eid, &rq, &rp) != OK) {
		log(LOG_FATAL, "block info mesg failed");
		return ERR;
	} else if (rp.info.ret != OK) {
		log(LOG_FATAL, "block info returned bad %i", rp.info.ret);
		return ERR;
	}

	log(LOG_INFO, "got info");

	*blk_size = rp.info.block_size;

	return OK;
}

	int
mbr_get_partition_info(int eid, int partition,
	size_t *p_start, size_t *p_len)
{
	size_t block_size;
	struct mbr *mbr;
	int ret, fid;
	size_t len;

	log(LOG_INFO, "reading mbr from 0x%x", eid);

	if (block_get_block_size(eid, &block_size) != OK) {
		return ERR;
	}
		
	log(LOG_INFO, "block size is 0x%x", block_size);

	len = PAGE_ALIGN(sizeof(struct mbr));
	fid = request_memory(len, 0x1000);
	if (fid < 0) {
		log(LOG_FATAL, "read mbr memory request failed");
		return ERR;
	}

	log(LOG_INFO, "read mbr");

	ret = block_read(eid, fid, 0, 1);
	if (ret != OK) {
		return ret;
	}

	mbr = frame_map_anywhere(fid);
	if (mbr == nil) {
		return ERR;
	}

	int i;
	for (i = 0; i < 4; i++) {
		log(LOG_INFO, "partition %i", i);
		log(LOG_INFO, "status = 0x%x", mbr->parts[i].status);
		log(LOG_INFO, "type = 0x%x", mbr->parts[i].type);
		log(LOG_INFO, "lba = 0x%x", mbr->parts[i].lba);
		log(LOG_INFO, "len = 0x%x", mbr->parts[i].sectors);
	}

	log(LOG_INFO, "partition %i starts at 0x%x and goes for 0x%x blocks",
			partition, 
			mbr->parts[partition].lba,
			mbr->parts[partition].sectors);

	*p_start = mbr->parts[partition].lba;
	*p_len = mbr->parts[partition].sectors;

	unmap_addr(fid, mbr);
	release_memory(fid);

	return OK;
}

