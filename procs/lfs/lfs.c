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
#include <crc.h>
#include <block.h>
#include <timer.h>
#include <log.h>

#include "lfs.h"

void
lfs_checkpoint_fill_segs(struct lfs *lfs, 
	struct lfs_b_checkpoint *checkpoint)
{
	struct lfs_segment *seg;
	uint32_t next;

	if (lfs->seg_head != nil) {
		checkpoint->seg_head = lfs->seg_head->index;
		checkpoint->seg_tail = lfs->seg_tail->index;
	} else {
		checkpoint->seg_head = lfs_seg_free;
		checkpoint->seg_tail = lfs_seg_free;
	}

	for (seg = lfs->seg_head; seg != nil; seg = seg->next) {
		if (seg->next != nil) {
			next = seg->next->index;
		} else {
			next = lfs_seg_free;
		}

		checkpoint->segs[seg->index] = next;
	}

	for (seg = lfs->seg_free; seg != nil; seg = seg->next) {
		checkpoint->segs[seg->index] = lfs_seg_free;
	}
}

int
lfs_write_checkpoint(struct lfs *lfs)
{
	struct lfs_b_checkpoint *checkpoint;

	log(LOG_INFO, "update checkpoint for timestamp %i", lfs->timestamp);

	checkpoint = frame_map_anywhere(lfs->checkpoint_fid);
	if (checkpoint == nil) {
		return ERR;
	}

	memset(checkpoint, 0, lfs->checkpoint_blks * lfs_blk_size);
	checkpoint->magic = lfs_checkpoint_magic;
	checkpoint->crc = 0;

	lfs_checkpoint_fill_segs(lfs, checkpoint);

	checkpoint->timestamp_a = lfs->timestamp;

	uint32_t *timestamp_b = (uint32_t *) 
		(((uint8_t *) checkpoint)
		+ sizeof(struct lfs_b_checkpoint)
		+ sizeof(uint32_t) * lfs->n_segs);

	*timestamp_b = lfs->timestamp;

	checkpoint->crc = crc_checksum((uint8_t *) checkpoint, 
		lfs->checkpoint_blks * lfs_blk_size);

	log(LOG_INFO, "checkpoint has crc 0x%x", checkpoint->crc);

	unmap_addr(lfs->checkpoint_fid, checkpoint);

	uint32_t blk = lfs->checkpoint_blk[lfs->timestamp % 2];
	
	if (block_write(lfs->blk_cid, lfs->checkpoint_fid, 
			lfs->part_off + blk * lfs->sec_per_blk, 
			lfs->checkpoint_blks * lfs->sec_per_blk) != OK) 
	{
		return ERR;
	}

	lfs->timestamp++;

	return OK;
}

int
lfs_format_write(struct lfs *lfs)
{
	struct lfs_b_superblock *super;
	int super_fid;

	super_fid = request_memory(lfs_blk_size, 0x1000);
	if (super_fid < 0) {
		return super_fid;
	}

	super = frame_map_anywhere(super_fid);
	if (super == nil) {
		release_memory(super_fid);
		return ERR;
	}

	memset(super, 0, lfs_blk_size);

	super->magic = lfs_superblock_magic;
	super->crc = 0;
	super->n_segs = lfs->n_segs;
	super->n_inodes = lfs->n_inodes;
	super->checkpoint_blks = lfs->checkpoint_blks;
	
	super->checkpoint_blk[0] = lfs->checkpoint_blk[0];
	super->checkpoint_blk[1] = lfs->checkpoint_blk[1];
	super->seg_start = lfs->seg_start;

	super->crc = crc_checksum((uint8_t *) super, lfs_blk_size);
	log(LOG_INFO, "super has crc 0x%x", super->crc);

	unmap_addr(super_fid, super);

	if (block_write(lfs->blk_cid, super_fid, 
			lfs->part_off, lfs->sec_per_blk) != OK) 
	{
		release_memory(super_fid);
		return ERR;
	}
	
	release_memory(super_fid);

	if (lfs_write_checkpoint(lfs) != OK) {
		return ERR;
	}

	return OK;
}

int
lfs_init(struct lfs *lfs)
{
	lfs->checkpoint_fid = request_memory(lfs->checkpoint_blks * lfs_blk_size, 0x1000);
	if (lfs->checkpoint_fid < 0) {
		lfs_free(lfs);
		return lfs->checkpoint_fid;
	}

	lfs->segments = malloc(sizeof(struct lfs_segment) * lfs->n_segs);
	if (lfs->segments == nil) {
		lfs_free(lfs);
		return ERR;
	}

	lfs->inodes = malloc(sizeof(struct lfs_inode) * lfs->n_inodes);
	if (lfs->inodes == nil) {
		lfs_free(lfs);
		return ERR;
	}

	struct lfs_segment *seg, *f;
	size_t i;

	lfs->seg_head = nil;
	lfs->seg_tail = nil;
	lfs->seg_free = nil;

	f = nil;
	for (i = 0; i < lfs->n_segs; i++) {
		seg = &lfs->segments[i];

		seg->index = i;
		seg->blk = lfs->seg_start + i * lfs_seg_blks;

		if (f == nil) {
			lfs->seg_free = seg;
			f = seg;
		} else {
			f->next = seg;
			f = seg;
		}

		seg->prev = nil;
		seg->next = nil;
	}

	struct lfs_inode *inode;
	for (i = 0; i < lfs->n_inodes; i++) {
		inode = &lfs->inodes[i];
		
		inode->inode_idx = i;
		inode->blk_cnt = 0;
		inode->len_real = 0;
		memset(inode->name, 0, sizeof(inode->name));

		inode->inode_blk = 0;
		inode->blks = nil;
	}

	return OK;
}

void
lfs_free(struct lfs *lfs)
{
	if (lfs->checkpoint_fid >= 0) release_memory(lfs->checkpoint_fid);
	if (lfs->segments != nil) free(lfs->segments);
	if (lfs->inodes != nil) free(lfs->inodes);

	lfs->checkpoint_fid = -1;
	lfs->segments = nil;
	lfs->inodes = nil;
}

int
lfs_setup(struct lfs *lfs,
	int blk_cid, size_t part_off, size_t part_len)
{
	lfs->blk_cid = blk_cid;
	lfs->part_off = part_off;
	lfs->part_len = part_len;

	lfs->checkpoint_fid = -1;
	lfs->segments = nil;
	lfs->inodes = nil;
	
	if (block_get_block_size(lfs->blk_cid, &lfs->sec_size) != OK) {
		return ERR;
	}
	
	lfs->sec_per_blk = lfs_blk_size / lfs->sec_size;

	log(LOG_INFO, "partition has len 0x%x and blk size 0x%x",
		part_len, lfs->sec_size);

	return OK;
}

int
lfs_load(struct lfs *lfs)
{
	if (lfs_init(lfs) != OK) {
		return ERR;
	}

	return ERR;
}

int
lfs_format(struct lfs *lfs)
{
	uint64_t useful_len;
	uint32_t n_segs_tent;

	lfs_free(lfs);

	lfs->seg_start = lfs_seg_blks;

	/* first segment worth for superblock and checkpoints */
	useful_len = lfs->part_len * lfs->sec_size
		- lfs->seg_start * lfs_blk_size; 

	n_segs_tent = useful_len / (lfs_seg_blks * lfs_blk_size);

	uint32_t checkpoint_size = 
		align_up(sizeof(struct lfs_b_checkpoint) 
			+ sizeof(uint32_t) 
			+ sizeof(uint32_t) * n_segs_tent,
		lfs_blk_size);

	if (checkpoint_size * 2 + lfs_blk_size > lfs_seg_blks * lfs_blk_size) {
		log(LOG_WARNING, "need more than one segment for metadata");
		lfs->seg_start = 
			align_up(checkpoint_size * 2 + lfs_blk_size,
					lfs_seg_blks * lfs_blk_size) / lfs_blk_size;
		log(LOG_WARNING, "seg start now 0x%x", lfs->seg_start);
	}

	useful_len = align_down(lfs->part_len * lfs->sec_size
			- lfs->seg_start * lfs_blk_size,
			lfs_seg_blks * lfs_blk_size);

	lfs->checkpoint_blks = checkpoint_size / lfs_blk_size;
	lfs->n_segs = useful_len / (lfs_seg_blks * lfs_blk_size);

	if (lfs->n_segs != n_segs_tent) {
		log(LOG_WARNING, "partition segment changed %i != tentative %i",
			lfs->n_segs, n_segs_tent);
	}

	log(LOG_INFO, "partition supports %i segments", lfs->n_segs);

	lfs->n_inodes = lfs->n_segs * (lfs_seg_blks - 1) / 2;
	log(LOG_INFO, "partition supports %i inodes", lfs->n_inodes);

	lfs->timestamp = 0;

	lfs->checkpoint_blk[0] = 1;
	lfs->checkpoint_blk[1] = lfs->checkpoint_blk[0] + lfs->checkpoint_blks;
	log(LOG_INFO, "put checkpoints at block %i and %i",
		lfs->checkpoint_blk[0], lfs->checkpoint_blk[1]);

	log(LOG_INFO, "put segments at block %i through %i",
		lfs->seg_start, lfs->seg_start + lfs->n_segs * lfs_seg_blks);

	if (lfs_init(lfs) != OK) {
		return ERR;
	}

	int r = lfs_format_write(lfs);

	release_memory(lfs->checkpoint_fid);

	return r;
}

