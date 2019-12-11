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

int
read_blocks(int eid, int fid,
		size_t start, size_t len)
{
	union block_req rq;
	union block_rsp rp;

	rq.read.type = BLOCK_read;
	rq.read.start = start;
	rq.read.len = len;

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

	ret = read_blocks(eid, fid, 0, block_size);
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

	fid = unmap_addr(mbr);
	release_memory(fid);

	return OK;
}

#if 0
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

	size_t
map_init_file(int block_eid, int partition, char *file_name,
	size_t pa, size_t len)
{
	struct fat_file *root, *f;
	struct fat fat;
	size_t rlen;
	int fid;

	if (fat_local_init(&fat, block_eid, partition) != OK) {
		log(LOG_FATAL, "fat_local_init failed");
		return 0;
	}

	root = &fat.files[FILE_root_fid];
	fid = fat_file_find(&fat, root, file_name);
	if (fid < 0) {
		log(LOG_FATAL, "failed to find %s", file_name);
		return 0;
	}

	f = &fat.files[fid];

	rlen = f->size;
	if (rlen > len) {
		rlen = len;
	}

	if (fat_file_read(&fat, f,
				pa, len,
				0, rlen) != OK) 
	{
		log(LOG_FATAL, "error reading init file");
		return 0;
	}

	fat_file_clunk(&fat, f);

	return rlen;
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

#endif

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
	int fid;

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

#if 0
	if (fat_local_init(&fat, block_eid, p_start, p_len) != OK) {
		log(LOG_FATAL, "fat_local_init failed");
		return;
	}

	root = &fat.files[FILE_root_fid];
	fid = fat_file_find(&fat, root, file_name);
	if (fid < 0) {
		log(LOG_FATAL, "failed to find %s", file_name);
		return;
	}

	f = &fat.files[fid];

	size_t pa, len, rlen;
	void *va;
	
	len = 0x1000;

	rlen = f->size;
	if (rlen > len) {
		rlen = len;
	}

	pa = request_memory(len);
	if (pa == nil) {
		log(LOG_FATAL, "failed to get memory for file");
		return;
	}

	if (fat_file_read(&fat, f,
				pa, len,
				0, rlen) != OK) 
	{
		log(LOG_FATAL, "error reading init file");
		return;
	}

	fat_file_clunk(&fat, f);

	va = map_addr(pa, len, MAP_RO);
	if (va == nil) {
		log(LOG_FATAL, "failed to map init file");
		return;
	}

	print_init_file(va, rlen);

	unmap_addr(va, len);
	release_addr(pa, len);
#endif
}

