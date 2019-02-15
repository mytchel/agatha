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
#include <fat.h>
#include <fs.h>

char *debug_name = "serial0";
int debug_pid;

char *init = "sdmmc1a:test";
char *init_file;
size_t init_pa, init_m_len, init_size;
int fat_fs_pid = 6;

void
debug(char *fmt, ...)
{
	uint8_t m[MESSAGE_LEN];
	va_list a;

	va_start(a, fmt);
	vsnprintf((char *) m, sizeof(m), fmt, a);
	va_end(a);

	mesg(debug_pid, m, m);
}

int
get_device_pid(char *name)
{
	union dev_reg_req rq;
	union dev_reg_rsp rp;

	rq.find.type = DEV_REG_find;
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
	union block_dev_req rq;
	union block_dev_rsp rp;
	int ret;

	if ((ret = give_addr(pid, pa, len)) != OK) {
		debug("block give_addr failed %i\n", ret);
		return ERR;
	}

	rq.read.type = BLOCK_DEV_read;
	rq.read.pa = pa;
	rq.read.len = len;
	rq.read.start = start;
	rq.read.r_len = r_len;

	if (mesg(pid, &rq, &rp) != OK) {
		debug("block send mesg failed\n");
		return ERR;
	} else if (rp.read.ret != OK) {
		debug("block read response bad %i\n", rp.read.ret);
		return ERR;
	}

	return OK;
}

	int
fat_local_init(struct fat *fat, int block_pid, int partition)
{
	union block_dev_req rq;
	union block_dev_rsp rp;
	struct mbr *mbr;
	size_t pa, len, block_size;
	int ret;

	fat->block_pid = block_pid;

	debug("reading mbr from %i\n", fat->block_pid);

	rq.info.type = BLOCK_DEV_info;
	
	if (mesg(block_pid, &rq, &rp) != OK) {
		debug("block info mesg failed\n");
		return ERR;
	} else if (rp.info.ret != OK) {
		debug("block info returned bad %i\n", rp.info.ret);
		return ERR;
	}

	block_size = rp.info.block_size;

	debug("block size is 0x%x\n", block_size);

	len = PAGE_ALIGN(sizeof(struct mbr));
	pa = request_memory(len);
	if (pa == nil) {
		debug("read mbr memory request failed\n");
		return ERR;
	}

	debug("read mbr\n");

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
		debug("partition %i\n", i);
		debug("status = 0x%x\n", mbr->parts[i].status);
		debug("type = 0x%x\n", mbr->parts[i].type);
		debug("lba = 0x%x\n", mbr->parts[i].lba);
		debug("len = 0x%x\n", mbr->parts[i].sectors);
	}

	debug("partition %i starts at 0x%x and goes for 0x%x blocks\n",
			partition, 
			mbr->parts[partition].lba,
			mbr->parts[partition].sectors);

	ret = fat_init(fat, block_pid, block_size,
			mbr->parts[partition].lba,
			mbr->parts[partition].sectors);

	if (ret != OK) {
		debug("error %i reading fat fs\n", ret);
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

	debug("loading init file %s\n", init);

	for (i = 0; init[i] && init[i] != ':'; i++)
		block_dev[i] = init[i];

	block_dev[i-1] = 0;
	file_name = &init[i+1];
	partition = init[i-1] - 'a';

	debug("find block device %s\n", block_dev);

	do {
		block_pid = get_device_pid(block_dev);
	} while (block_pid < 0);

	debug("mount fat fs %s pid %i parititon %i to read %s\n",
			block_dev, block_pid, partition, file_name);

	if (fat_local_init(&fat, block_pid, partition) != OK) {
		debug("fat_local_init failed\n");
		return ERR;
	}

	root = &fat.files[FILE_root_fid];
	fid = fat_file_find(&fat, root, file_name);
	if (fid < 0) {
		debug("failed to find %s\n", file_name);
		return ERR;
	}

	f = &fat.files[fid];

	init_m_len = PAGE_ALIGN(f->dsize);
	init_size = f->size;

	init_pa = request_memory(init_m_len);
	if (init_pa == nil) {
		debug("request for 0x%x bytes failed\n", init_m_len);
		return ERR;
	}

	if (fat_file_read(&fat, f,
				init_pa, init_m_len,
				0, init_size) != OK) {
		debug("error reading init file\n");
		return ERR;
	}

	init_file = map_addr(init_pa, init_m_len, MAP_RO);
	if (file == nil) {
		debug("failed to map init file\n");
		return ERR;
	}

	fat_file_clunk(&fat, f);

	return OK;
}

	int
read_init_file(char *f, size_t size)
{
	debug("processing init file\n");

	debug("init file contains:\n---\n");

	int i;
	for (i = 0; i < init_size; i++)
		debug("%c", init_file[i]);

	debug("---\n");

	return OK;
}

	void
main(void)
{
	do {
		debug_pid = get_device_pid(debug_name);
	} while (debug_pid < 0);

	debug("init using %s for debug\n", debug_name);

	if (map_init_file(init) != OK) {
		raise();
	}

	if (read_init_file(init_file, init_size) != OK) {
		raise();
	}

	unmap_addr(init_file, init_m_len);
	release_addr(init_pa, init_m_len);

	uint8_t m[MESSAGE_LEN];
	while (true)
		recv(-1, m);
}

