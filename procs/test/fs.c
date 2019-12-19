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

int
read_blocks(int eid, int fid,
		size_t blk, size_t n)
{
	union block_req rq;
	union block_rsp rp;

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

size_t
block_get_block_size(int eid)
{
	union block_req rq;
	union block_rsp rp;

	rq.info.type = BLOCK_info;
	
	if (mesg(eid, &rq, &rp) != OK) {
		log(LOG_FATAL, "block info mesg failed");
		return 0;
	} else if (rp.info.ret != OK) {
		log(LOG_FATAL, "block info returned bad %i", rp.info.ret);
		return 0;
	}

	log(LOG_INFO, "got info");

	return rp.info.block_size;
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

	block_size = block_get_block_size(eid);
	if (block_size == 0) {
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

	ret = read_blocks(eid, fid, 0, 1);
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

	int
fat_local_init(struct fat *fat, int eid, 
	size_t p_start, size_t p_len)
{
	size_t block_size;
	int ret;

	block_size = block_get_block_size(eid);
	if (block_size == 0) {
		return ERR;
	}
		
	ret = fat_init(fat, eid, block_size, p_start, p_len);
	if (ret != OK) {
		log(LOG_FATAL, "error %i initing fat fs", ret);
	}

	return ret;
}

	void
print_init_file(char *f, size_t size)
{
	char line[50];
	log(LOG_INFO, "processing init file");

	log(LOG_INFO, "---");

	int i, l = 0;
	for (i = 0; i < size; i++) {
		line[l++] = f[i];
		
		if (f[i] == '\n') {
			line[l-1] = 0;
			log(LOG_INFO, "%s", line);
			l = 0;
		}
	}

	log(LOG_INFO, "---");
}

void
test_fat_fs(void)
{
	int partition = 0;
	char *file_name = "test";

	union proc0_req prq;
	union proc0_rsp prp;

	size_t p_start, p_len;

	struct fat_file *root, *f;
	struct fat fat;
	void *va;
	int fid;
	int cid;

	prq.get_resource.type = PROC0_get_resource;
	prq.get_resource.resource_type = RESOURCE_type_block;
	
	int block_eid = kcap_alloc();
	if (block_eid < 0) {
		log(LOG_FATAL, "failed alloc cap");
		exit(ERR);
	}

	mesg_cap(CID_PARENT, &prq, &prp, block_eid);
	if (prp.get_resource.ret != OK) {
		log(LOG_FATAL, "failed to get block device");
		exit(ERR);
	}

	if (mbr_get_partition_info(block_eid, partition, 
			&p_start, &p_len) != OK) 
	{
		exit(ERR);
	}

	if (fat_local_init(&fat, block_eid, p_start, p_len) != OK) {
		log(LOG_FATAL, "fat_local_init failed");
		exit(ERR);
	}

	root = &fat.files[FILE_root_fid];
	fid = fat_file_find(&fat, root, file_name);
	if (fid < 0) {
		log(LOG_FATAL, "failed to find %s", file_name);
		exit(ERR);
	}

	f = &fat.files[fid];
	
	cid = request_memory(PAGE_ALIGN(f->size), 0x1000);
	if (cid < 0) {
		log(LOG_FATAL, "failed to get memory for file");
		exit(ERR);
	}

	if (fat_file_read(&fat, f,
				cid, 0,
				0, f->size) != OK) 
	{
		log(LOG_FATAL, "error reading init file");
		exit(ERR);
	}

	va = frame_map_anywhere(cid);
	if (va == nil) {
		log(LOG_FATAL, "failed to map init file");
		exit(ERR);
	}

	print_init_file(va, f->size);
	
	fat_file_clunk(&fat, f);

	unmap_addr(cid, va);
	release_memory(cid);
}

