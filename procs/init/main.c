#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <proc0.h>
#include <mmu.h>
#include <stdarg.h>
#include <string.h>
#include <dev_reg.h>
#include <block.h>
#include <mbr.h>
#include <fs.h>
#include <fat.h>
#include <log.h>

#if 0
char *init = "sd0a:test";
char *init_file;
size_t init_pa, init_m_len, init_size;

int
get_device_pid(char *name)
{
	union dev_reg_req rq;
	union dev_reg_rsp rp;

	rq.find.type = DEV_REG_find_req;
	rq.find.block = true;
	snprintf(rq.find.name, sizeof(rq.find.name),
			"%s", name);

	if (mesg(DEV_REG_PID, &rq, &rp) != OK) {
		return ERR;
	} else if (rp.find.ret != OK) {
		return ERR;
	}

	return rp.find.pid;
}

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

	rq.read.type = BLOCK_read_req;
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

	int
fat_local_init(struct fat *fat, int block_pid, int partition)
{
	union block_req rq;
	union block_rsp rp;
	struct mbr *mbr;
	size_t pa, len, block_size;
	int ret;

	fat->block_pid = block_pid;

	log(LOG_INFO, "reading mbr from %i", fat->block_pid);

	rq.info.type = BLOCK_info_req;
	
	if (mesg(block_pid, &rq, &rp) != OK) {
		log(LOG_FATAL, "block info mesg failed");
		return ERR;
	} else if (rp.info.ret != OK) {
		log(LOG_FATAL, "block info returned bad %i", rp.info.ret);
		return ERR;
	}

	block_size = rp.info.block_size;

	log(LOG_INFO, "block size is 0x%x", block_size);

	len = PAGE_ALIGN(sizeof(struct mbr));
	pa = request_memory(len);
	if (pa == nil) {
		log(LOG_FATAL, "read mbr memory request failed");
		return ERR;
	}

	log(LOG_INFO, "read mbr");

	ret = read_blocks(block_pid, pa, len, 0, block_size);
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

	ret = fat_init(fat, block_pid, block_size,
			mbr->parts[partition].lba,
			mbr->parts[partition].sectors);

	if (ret != OK) {
		log(LOG_FATAL, "error %i reading fat fs", ret);
	}

	unmap_addr(mbr, len);	
	release_addr(pa, len);

	return ret;
}

	int
map_init_file(char *file)
{
	char block_dev[32], *file_name;
	int i, block_pid, partition, fid;
	struct fat_file *root, *f;
	struct fat fat;

	log(LOG_INFO, "load %s", init);

	for (i = 0; init[i] && init[i] != ':'; i++)
		block_dev[i] = init[i];

	block_dev[i-1] = 0;
	file_name = &init[i+1];
	partition = init[i-1] - 'a';

	log(LOG_INFO, "find block device %s", block_dev);

	do {
		block_pid = get_device_pid(block_dev);
	} while (block_pid < 0);

	log(LOG_INFO, "mount fat fs %s pid %i parititon %i to read %s",
			block_dev, block_pid, partition, file_name);

	if (fat_local_init(&fat, block_pid, partition) != OK) {
		log(LOG_FATAL, "fat_local_init failed");
		return ERR;
	}

	root = &fat.files[FILE_root_fid];
	fid = fat_file_find(&fat, root, file_name);
	if (fid < 0) {
		log(LOG_FATAL, "failed to find %s", file_name);
		return ERR;
	}

	f = &fat.files[fid];

	init_m_len = PAGE_ALIGN(f->dsize);
	init_size = f->size;

	init_pa = request_memory(init_m_len);
	if (init_pa == nil) {
		log(LOG_FATAL, "request for 0x%x bytes failed", init_m_len);
		return ERR;
	}

	if (fat_file_read(&fat, f,
				init_pa, init_m_len,
				0, init_size) != OK) {
		log(LOG_FATAL, "error reading init file");
		return ERR;
	}

	init_file = map_addr(init_pa, init_m_len, MAP_RO);
	if (file == nil) {
		log(LOG_FATAL, "failed to map init file");
		return ERR;
	}

	fat_file_clunk(&fat, f);

	return OK;
}

	int
read_init_file(char *f, size_t size)
{
	char line[50];
	log(LOG_INFO, "processing init file");

	log(LOG_INFO, "---");

	int i, l = 0;
	for (i = 0; i < init_size; i++) {
		line[l++] = init_file[i];
		
		if (init_file[i] == '\n') {
			line[l-1] = 0;
			log(LOG_INFO, "%s", line);
			l = 0;
		}
	}

	log(LOG_INFO, "---");

	return OK;
}

#endif

	void
main(int p_eid)
{
	parent_eid = p_eid;

	log_init("init");

	log(LOG_INFO, "init starting");

#if 1
	
#endif

#if 0	
	if (map_init_file(init) != OK) {
		exit(1);
	}

	if (read_init_file(init_file, init_size) != OK) {
		exit(2);
	}

	unmap_addr(init_file, init_m_len);
	release_addr(init_pa, init_m_len);
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

