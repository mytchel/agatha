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
#include <timer.h>
#include <log.h>

#include "lfs.h"

int
block_read(int cid, int fid, size_t start, size_t len);

int
block_write(int cid, int fid, size_t start, size_t len);

int
mbr_get_partition_info(int cid, int partition,
	size_t *p_start, size_t *p_len);

	int
main(void)
{
	log_init("fs");

	int partition = 1;

	log(LOG_INFO, "fs starting on partition %i", partition);

	struct lfs lfs;

	int mount_cid;
	int blk_cid;
	int timer_cid;

	mount_cid = kcap_alloc();
	blk_cid = kcap_alloc();
	timer_cid = kcap_alloc();

	if (mount_cid < 0 || blk_cid < 0 || timer_cid < 0) {
		log(LOG_FATAL, "cap alloc failed");
		return ERR;
	}

	union proc0_req prq;
	union proc0_rsp prp;

	prq.get_resource.type = PROC0_get_resource;
	prq.get_resource.resource_type = RESOURCE_type_mount;
	if (mesg_cap(CID_PARENT, &prq, &prp, mount_cid) != OK) {
		return ERR;
	} else if (prp.get_resource.ret != OK) {
		log(LOG_FATAL, "failed to get resource");
		return ERR;
	}

	prq.get_resource.type = PROC0_get_resource;
	prq.get_resource.resource_type = RESOURCE_type_block;
	if (mesg_cap(CID_PARENT, &prq, &prp, blk_cid) != OK) {
		return ERR;
	} else if (prp.get_resource.ret != OK) {
		log(LOG_FATAL, "failed to get resource");
		return ERR;
	}

	prq.get_resource.type = PROC0_get_resource;
	prq.get_resource.resource_type = RESOURCE_type_timer;
	if (mesg_cap(CID_PARENT, &prq, &prp, timer_cid) != OK) {
		return ERR;
	} else if (prp.get_resource.ret != OK) {
		log(LOG_FATAL, "failed to get resource");
		return ERR;
	}

	size_t p_start, p_len;

	if (mbr_get_partition_info(blk_cid, partition, &p_start, &p_len) != OK) {
		return ERR;
	}

	log(LOG_INFO, "format lfs partition");

	if (lfs_setup(&lfs, blk_cid, p_start, p_len) != OK) {
		return ERR;
	}

	if (lfs_format(&lfs) != OK) {
		return ERR;
	}

	uint8_t name[16] = { 0 };
	snprintf(name, sizeof(name), "test");

	if (lfs_create(&lfs, name) != OK) {
		return ERR;
	}

	uint8_t buf[32];
	
	snprintf(buf, sizeof(buf), "one two three four five");

	size_t l = strlen(buf);
	
	if (lfs_write(&lfs, name, buf, 0, sizeof(buf)) != OK) {
		return ERR;
	}

	memset(buf, 0, sizeof(buf));

	if (lfs_read(&lfs, name, buf, 0, sizeof(buf)) != OK) {
		return ERR;
	}

	buf[l] = 0;

	log(LOG_INFO, "read: '%s'", buf);

	lfs_flush(&lfs);
	lfs_free(&lfs);

	log(LOG_INFO, "load lfs partition");
	
	if (lfs_setup(&lfs, blk_cid, p_start, p_len) != OK) {
		return ERR;
	}

	if (lfs_load(&lfs) != OK) {
		return ERR;
	}

	memset(buf, 0, sizeof(buf));
	if (lfs_read(&lfs, name, buf, 0, sizeof(buf)) != OK) {
		return ERR;
	}

	log(LOG_INFO, "read: '%s'", buf);

	log(LOG_INFO, "trunc", buf);
	if (lfs_trunc(&lfs, name, 10) != OK) {
		return ERR;
	}

	log(LOG_INFO, "read after trunc");
	memset(buf, 0, sizeof(buf));
	if (lfs_read(&lfs, name, buf, 0, 10) != OK) {
		return ERR;
	}
	
	log(LOG_INFO, "read: '%s'", buf);

	log(LOG_INFO, "rm");
	if (lfs_rm(&lfs, name) != OK) {
		return ERR;
	}
	
	memset(buf, 0, sizeof(buf));
	if (lfs_read(&lfs, name, buf, 0, 10) != OK) {
		log(LOG_INFO, "read after rm failed");
	} else {
		log(LOG_WARNING, "read after rm: '%s'", buf);
	}

	lfs_free(&lfs);

	log(LOG_INFO, "lfs test done");

	return OK;
}

