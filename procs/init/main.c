#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <dev_reg.h>
#include <fs.h>

char *debug_name = "serial0";
int debug_pid;

char *init = "sdmmc0a:test";
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
map_init_file(char *file)
{
	char block_dev[32], *file_name;
	int i, block_pid, partition, file_fid;
	union file_req rq;
	union file_rsp rp;

	debug("loading init file %s\n", init);

	for (i = 0; init[i] && init[i] != ':'; i++)
		block_dev[i] = init[i];

	block_dev[i-1] = 0;
	file_name = &init[i+1];
	partition = init[i-1] - 'a';

	do {
		block_pid = get_device_pid(block_dev);
	} while (block_pid < 0);

	debug("mount fat fs %s pid %i parititon %i to read %s\n",
			block_dev, block_pid, partition, file_name);

	rq.start.type = FILE_start;
	rq.start.dev_pid = block_pid;
	rq.start.partition = partition;

	if (send(fat_fs_pid, &rq) != OK) {
		debug("failed to send fs start to %i\n", fat_fs_pid);
		return ERR;
	} else if (recv(fat_fs_pid, &rp) != fat_fs_pid) {
		debug("failed to recv fs start from %i\n", fat_fs_pid);
		return ERR;
	} else if (rp.start.ret != OK) {
		debug("fs start returned bad %i\n", rp.start.ret);
		return ERR;
	}

	rq.find.type = FILE_find;
	rq.find.fid = FILE_root_fid;
	memcpy(rq.find.name, file_name, sizeof(rq.find.name));

	if (send(fat_fs_pid, &rq) != OK) {
		debug("failed to send fs find to %i\n", fat_fs_pid);
		return ERR;
	} else if (recv(fat_fs_pid, &rp) != fat_fs_pid) {
		debug("failed to recv fs find from %i\n", fat_fs_pid);
		return ERR;
	} else if (rp.start.ret != OK) {
		debug("fs find returned bad %i\n", rp.start.ret);
		return ERR;
	}

	file_fid = rp.find.fid;

	rq.stat.type = FILE_stat;
	rq.stat.fid = file_fid;

	if (send(fat_fs_pid, &rq) != OK) {
		debug("failed to send fs stat to %i\n", fat_fs_pid);
		return ERR;
	} else if (recv(fat_fs_pid, &rp) != fat_fs_pid) {
		debug("failed to recv fs stat from %i\n", fat_fs_pid);
		return ERR;
	} else if (rp.start.ret != OK) {
		debug("fs stat returned bad %i\n", rp.start.ret);
		return ERR;
	}

	init_m_len = PAGE_ALIGN(rp.stat.dsize);
	init_size = rp.stat.size;

	init_pa = request_memory(init_m_len);
	if (init_pa == nil) {
		debug("request for 0x%h bytes failed\n", init_m_len);
		return ERR;
	}

	rq.read.type = FILE_read;
	rq.read.fid = file_fid;
	rq.read.offset = 0;
	rq.read.r_len = init_size;
	rq.read.m_len = init_m_len;
	rq.read.pa = init_pa;

	if (give_addr(fat_fs_pid, init_pa, init_m_len) != OK) {
		debug("failed to give fs 0x%h 0x%h\n", init_pa, init_m_len);
		return ERR;
	}

	if (send(fat_fs_pid, &rq) != OK) {
		debug("failed to send fs read to %i\n", fat_fs_pid);
		return ERR;
	} else if (recv(fat_fs_pid, &rp) != fat_fs_pid) {
		debug("failed to recv fs read from %i\n", fat_fs_pid);
		return ERR;
	} else if (rp.start.ret != OK) {
		debug("fs read returned bad %i\n", rp.start.ret);
		return ERR;
	}

	init_file = map_addr(init_pa, init_m_len, MAP_RO);
	if (file == nil) {
		debug("failed to map init file\n");
		return ERR;
	}

	rq.clunk.type = FILE_clunk;
	rq.clunk.fid = file_fid;

	if (send(fat_fs_pid, &rq) != OK) {
		debug("failed to send fs clunk to %i\n", fat_fs_pid);
		return ERR;
	} else if (recv(fat_fs_pid, &rp) != fat_fs_pid) {
		debug("failed to recv fs clunk from %i\n", fat_fs_pid);
		return ERR;
	} else if (rp.start.ret != OK) {
		debug("fs clunk returned bad %i\n", rp.start.ret);
		return ERR;
	}

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
	while (true)
		;

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

	while (true)
		yield();
}

