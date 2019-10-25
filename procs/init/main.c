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
read_blocks(int pid, size_t pa, size_t len,
		size_t start, size_t r_len)
{
	union block_req rq;
	union block_rsp rp;
	int ret;

	if ((ret = give_addr(pid, pa, len)) != OK) {
		log(LOG_FATAL, "block give_addr failed %i", ret);
		return ERR;
	}

	rq.read.type = BLOCK_read;
	rq.read.pa = pa;
	rq.read.len = len;
	rq.read.start = start;
	rq.read.r_len = r_len;

	if (mesg(pid, &rq, &rp) != OK) {
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

	return rp.info.block_size;
}

	int
mbr_get_parition_info(int eid, int partition,
	size_t *p_start, size_t *p_len)
{
	size_t block_size;
	struct mbr *mbr;
	size_t pa, len;
	int ret;

	log(LOG_INFO, "reading mbr from %i", eid);

	block_size = block_get_block_size(eid);
	if (block_size == 0) {
		return ERR;
	}
		
	log(LOG_INFO, "block size is 0x%x", block_size);

	len = PAGE_ALIGN(sizeof(struct mbr));
	pa = request_memory(len);
	if (pa == nil) {
		log(LOG_FATAL, "read mbr memory request failed");
		return ERR;
	}

	log(LOG_INFO, "read mbr");

	ret = read_blocks(eid, pa, len, 0, block_size);
	if (ret != OK) {
		return ret;
	}

	mbr = map_addr(pa, len, MAP_RO);
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

	unmap_addr(mbr, len);
	release_addr(pa, len);

	return OK;
}

	int
fat_local_init(struct fat *fat, int eid, int partition)
{
	size_t p_start, p_len;
	size_t block_size;
	int ret;

	block_size = block_get_block_size(eid);
	if (block_size == 0) {
		return ERR;
	}
		
	if (mbr_get_parition_info(eid, partition, &p_start, &p_len) != OK) {
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

void
test_fat_fs(void)
{
	int partition = 0;
	char *file_name = "test";

	union proc0_req prq;
	union proc0_rsp prp;

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

	if (fat_local_init(&fat, block_eid, partition) != OK) {
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
}

void
test_timer(void)
{
	union proc0_req prq;
	union proc0_rsp prp;

	prq.get_resource.type = PROC0_get_resource;
	prq.get_resource.resource_type = RESOURCE_type_timer;
	
	int timer_eid = kcap_alloc();
	if (timer_eid < 0) {
		exit(ERR);
	}

	mesg_cap(CID_PARENT, &prq, &prp, timer_eid);
	if (prp.get_resource.ret != OK) {
		exit(ERR);
	}

	int timer_lid = kobj_alloc(OBJ_endpoint, 1);
	if (timer_lid < 0) {
		exit(ERR);
	}
		
	int timer_leid = kcap_alloc();
	if (timer_leid < 0) {
		exit(ERR);
	}

	if (endpoint_connect(timer_lid, timer_leid) != OK) {
		exit(ERR);
	}

	union timer_req rq;
	union timer_rsp rp;

	rq.create.type = TIMER_create;
	rq.create.signal = 0x111;

	mesg_cap(timer_eid, &rq, &rp, timer_leid);
	if (rp.create.ret != OK) {
		exit(ERR);
	}

	int timer_id = rp.create.id;

	int i;
	for (i = 0; i < 2; i++) {
		rq.set.type = TIMER_set;
		rq.set.id = timer_id;
		rq.set.time_ms = 5 * 1000;

		mesg(timer_eid, &rq, &rp);
		if (rp.set.ret != OK) {
			exit(ERR);
		}

		uint8_t t[MESSAGE_LEN];
		int t_pid;

		while (true) {
			recv(timer_lid, &t_pid, t);
			if (t_pid == PID_SIGNAL) {
				uint32_t signal = ((uint32_t *) t)[0];
				log(LOG_INFO, "got timer signal 0x%x", signal);
				break;
			} else {
				log(LOG_WARNING, "how did we get another message?");
			}
		}
	}
}

	void
main(void)
{
	log_init("init");

	log(LOG_INFO, "init starting");

#if 1
	test_timer();
#endif

#if 1
	test_fat_fs();
#endif

#if 0
	uint32_t **p;

	p = malloc(10 * sizeof(uint32_t));
	if (p == nil) {
		log(LOG_FATAL, "error allocating pointer buffer");
		exit(3);
	}

	size_t i, j;
	for (i = 1; i < (1<<15); i *= 2) {
		for (j = 0; j < 10; j++) {
			p[j] = malloc(i + j);
			if (p[j] == nil) {
				log(LOG_FATAL, "error allocating buffer %i size 0x%x",
						j, i + j);
			} else {
				log(LOG_INFO, "buffer %i of size 0x%x at 0x%x", j, i + j, p[j]);
				memset(p[j], j, i + j);
			}
		}	

		for (j = 0; j < 10; j++) {
			log(LOG_INFO, "free buffer %i at 0x%x", j, p[j]);
			free(p[j]);
		}
	}

	log(LOG_INFO, "alloc test finished!");
#endif

	exit(OK);
}

