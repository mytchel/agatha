#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <stdarg.h>
#include <string.h>
#include <log.h>
#include <dev_reg.h>
#include <block_dev.h>
#include <mbr.h>
#include <fs.h>
#include <fat.h>

static int
fat_get_device_pid(char *name)
{
	union dev_reg_req rq;
	union dev_reg_rsp rp;

	rq.find.type = DEV_REG_find;
	snprintf(rq.find.name, sizeof(rq.find.name),
			"%s", name);

	if (mesg(DEV_REG_PID, &rq, &rp) != OK) {
		return ERR;
	} else if (rp.find.ret != OK) {
		return ERR;
	}

	return rp.find.pid;
}

	static int
handle_find(struct fat *fat, int from, 
		union file_req *rq,
		union file_rsp *rp)
{
	struct fat_file *parent;

	log(LOG_INFO, "fat fs find %i %s", rq->find.fid, rq->find.name);

	if (0 > rq->find.fid || rq->find.fid > FIDSMAX) {
		log(LOG_WARNING, "fid bad");
	 return ERR;
	}

	parent = &fat->files[rq->find.fid];
	if (parent->refs == 0) {
		log(LOG_WARNING, "parent file bad");
		return ERR;
	}

	rp->find.fid = fat_file_find(fat,
			parent, rq->find.name);

	if (rp->find.fid > 0) {
		log(LOG_INFO, "found");
		parent->refs++;
		return OK;
	} else {
		return ERR;
	}
}

	static int
handle_stat(struct fat *fat, int from, 
		union file_req *rq,
		union file_rsp *rp)
{
	struct fat_file *f;

	if (0 > rq->stat.fid || rq->stat.fid > FIDSMAX) {
	 return ERR;
	}

	f = &fat->files[rq->stat.fid];
	if (f->refs == 0) {
		return ERR;
	}

	rp->stat.attr = f->attr;
	rp->stat.size = f->size;
	rp->stat.dsize = f->dsize;

	return OK;
}

	static int
handle_read(struct fat *fat, int from, 
		union file_req *rq,
		union file_rsp *rp)
{
	struct fat_file *f;
	int ret;

	if (0 > rq->read.fid || rq->read.fid > FIDSMAX) {
	 return ERR;
	}

	f = &fat->files[rq->read.fid];
	if (f->refs == 0) {
		return ERR;
	}

	ret = fat_file_read(fat, f,
			rq->read.pa, rq->read.m_len, 
			rq->read.offset, rq->read.r_len);

	give_addr(from, rq->read.pa, rq->read.m_len);

	return ret;
}

	static int
handle_write(struct fat *fat, int from, 
		union file_req *rq,
		union file_rsp *rp)
{
	return ERR;
}

	static int
handle_clunk(struct fat *fat, int from, 
		union file_req *rq,
		union file_rsp *rp)
{
	struct fat_file *f;

	if (0 > rq->clunk.fid || rq->clunk.fid > FIDSMAX) {
	 return ERR;
	}

	f = &fat->files[rq->clunk.fid];
	if (f->refs == 0) {
		return ERR;
	}

	fat_file_clunk(fat, f);
	return OK;
}

int
fat_mount(struct fat *fat) 
{
	union file_req rq;
	union file_rsp rp;
	int from;

	while (true) {
		if ((from = recv(-1, &rq)) < 0)
			continue;

		rp.untyped.type = rq.type;
		switch (rq.type) {
			case FILE_find:
				rp.untyped.ret = handle_find(fat, from, &rq, &rp);
				break;

			case FILE_stat:
				rp.untyped.ret = handle_stat(fat, from, &rq, &rp);
				break;

			case FILE_read:
				rp.untyped.ret = handle_read(fat, from, &rq, &rp);
				break;

			case FILE_write:
				rp.untyped.ret = handle_write(fat, from, &rq, &rp);
				break;

			case FILE_clunk:
				rp.untyped.ret = handle_clunk(fat, from, &rq, &rp);
				break;

			default:
				rp.untyped.ret = ERR;
				break;
		};

		send(from, &rp);
	}
		
	return OK;
}

int
main(void)
{
	char *block_dev = "sdmmc0";
	int partition = 0;

	union block_dev_req rq;
	union block_dev_rsp rp;
	size_t pa, len, block_size;
	int ret, block_pid;
	struct mbr *mbr;
	struct fat fat;
	
	do {
		fat_debug_pid = fat_get_device_pid(fat_debug_name);
	} while (fat_debug_pid < 0);

	log(LOG_INFO, "fatfs mounting %s.%i", block_dev, partition);

	do {
		block_pid = fat_get_device_pid(block_dev);
	} while (block_pid < 0);

	log(LOG_INFO, "mount fat fs %s pid %i parititon %i",
			block_dev, block_pid, partition);

	log(LOG_INFO, "reading mbr from %i", block_pid);

	rq.info.type = BLOCK_DEV_info;
	
	if (mesg(block_pid, &rq, &rp) != OK) {
		log(LOG_WARNING, "block info mesg failed");
		return ERR;
	} else if (rp.info.ret != OK) {
		log(LOG_WARNING, "block info returned bad %i", rp.info.ret);
		return ERR;
	}

	block_size = rp.info.block_len;

	len = PAGE_ALIGN(sizeof(struct mbr));
	pa = request_memory(len);
	if (pa == nil) {
		log(LOG_WARNING, "read mbr memory request failed");
		return ERR;
	}

	if ((ret = give_addr(block_pid, pa, len)) != OK) {
		log(LOG_WARNING, "give block addr failed");
		return ERR;
	}

	rq.read.type = BLOCK_DEV_read;
	rq.read.pa = pa;
	rq.read.len = len;
	rq.read.start = 0;
	rq.read.r_len = block_size;

	if (mesg(block_pid, &rq, &rp) != OK) {
		log(LOG_WARNING, "block read mesg failed");
		return ERR;
	} else if (rp.read.ret != OK) {
		log(LOG_WARNING, "block read returned bad %i", rp.read.ret);
		return ERR;
	}

	mbr = map_addr(pa, len, MAP_RO);
	if (mbr == nil) {
		return ERR;
	}

	log(LOG_INFO, "partition %i starts at 0x%x and goes for 0x%x blocks",
				partition, 
				mbr->parts[partition].lba,
				mbr->parts[partition].sectors);

	ret = fat_init(&fat, block_pid,
			block_size,
			mbr->parts[partition].lba,
			mbr->parts[partition].sectors);
	
	if (ret != OK) {
			log(LOG_WARNING, "error %i init fat fs", ret);
	}

	unmap_addr(mbr, len);	
	release_addr(pa, len);

	fat_mount(&fat);

	raise();
}

