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

	if (lfs_setup(&lfs, blk_cid, p_start, p_len) != OK) {
		return ERR;
	}

	if (lfs_format(&lfs) != OK) {
		return ERR;
	}

	lfs_free(&lfs);

	return OK;
}

