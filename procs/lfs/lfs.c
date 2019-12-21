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

	log(LOG_INFO, "checkpoint fill segs");

	if (lfs->seg_head != nil) {
		log(LOG_INFO, "checkpoint head %i and tail %i",
			lfs->seg_head->index, lfs->seg_tail->index);

		checkpoint->seg_head = lfs->seg_head->index;
		checkpoint->seg_tail = lfs->seg_tail->index;
	} else {
		log(LOG_INFO, "checkpoint no list");

		checkpoint->seg_head = lfs_seg_free;
		checkpoint->seg_tail = lfs_seg_free;
	}

	for (seg = lfs->seg_head; seg != nil; seg = seg->next) {
		if (seg->next != nil) {
			next = seg->next->index;
		} else {
			next = lfs_seg_end;
		}

		log(LOG_INFO, "checkpoint seg 0x%x followed by 0x%x",	
			seg->index, next);

		checkpoint->segs[seg->index] = next;
	}

	for (seg = lfs->seg_free; seg != nil; seg = seg->next) {
		log(LOG_INFO, "checkpoint seg 0x%x is free",	
			seg->index);

		checkpoint->segs[seg->index] = lfs_seg_free;
	}
}

int
lfs_write_checkpoint(struct lfs *lfs)
{
	struct lfs_b_checkpoint *checkpoint;

	log(LOG_INFO, "update checkpoint for timestamp %i", 
		lfs->checkpoint_timestamp);

	checkpoint = frame_map_anywhere(lfs->checkpoint_fid);
	if (checkpoint == nil) {
		return ERR;
	}

	memset(checkpoint, 0, lfs->checkpoint_blks * lfs_blk_size);
	checkpoint->magic = lfs_checkpoint_magic;
	checkpoint->crc = 0;

	lfs_checkpoint_fill_segs(lfs, checkpoint);

	checkpoint->timestamp = lfs->checkpoint_timestamp;

	checkpoint->crc = crc_checksum((uint8_t *) checkpoint, 
		lfs->checkpoint_blks * lfs_blk_size);

	log(LOG_INFO, "checkpoint has crc 0x%x", checkpoint->crc);

	unmap_addr(lfs->checkpoint_fid, checkpoint);

	uint32_t blk = lfs->checkpoint_blk[lfs->checkpoint_timestamp % 2];
	
	if (block_write(lfs->blk_cid, lfs->checkpoint_fid, 
			lfs->part_off + blk * lfs->sec_per_blk, 
			lfs->checkpoint_blks * lfs->sec_per_blk) != OK) 
	{
		return ERR;
	}

	lfs->checkpoint_timestamp++;

	return OK;
}

struct lfs_b_checkpoint *
lfs_load_checkpoint_data(struct lfs *lfs, size_t idx)
{
	struct lfs_b_checkpoint *checkpoint;
	uint32_t crc_store, crc_expect;

	log(LOG_INFO, "load checkpoint %i", idx);

	if (block_read(lfs->blk_cid, lfs->checkpoint_fid, 
			lfs->part_off + lfs->checkpoint_blk[idx] * lfs->sec_per_blk, 
			lfs->checkpoint_blks * lfs->sec_per_blk) != OK) 
	{
		return nil;
	}
	
	checkpoint = frame_map_anywhere(lfs->checkpoint_fid);
	if (checkpoint == nil) {
		return nil;
	}

	if (checkpoint->magic != lfs_checkpoint_magic) {
		log(LOG_FATAL, "checkpoint %i magic bad 0x%x != expected 0x%x",
			idx, checkpoint->magic, lfs_checkpoint_magic);
		unmap_addr(lfs->checkpoint_fid, checkpoint);
		return nil;
	}

	crc_store = checkpoint->crc;
	checkpoint->crc = 0;
	crc_expect = crc_checksum((uint8_t *) checkpoint, 
		lfs->checkpoint_blks * lfs_blk_size);

	if (crc_store != crc_expect) {
		log(LOG_FATAL, "checkpoint %i crc bad 0x%x != expected 0x%x",
			idx, crc_store, crc_expect);
		unmap_addr(lfs->checkpoint_fid, checkpoint);
		return nil;
	}

	return checkpoint;
}

int
lfs_process_checkpoint(struct lfs *lfs, 
	struct lfs_b_checkpoint *checkpoint)
{
	struct lfs_segment *seg, *f;
	uint32_t i;

	log(LOG_INFO, "process checkpoint");

	if (checkpoint->seg_head == lfs_seg_free) {
		log(LOG_INFO, "no list");

		lfs->seg_head = nil;
		lfs->seg_tail = nil;

	} else {
		log(LOG_INFO, "list start at %i end at %i",
			checkpoint->seg_head, checkpoint->seg_tail);

		lfs->seg_head = &lfs->segments[checkpoint->seg_head];
		lfs->seg_tail = &lfs->segments[checkpoint->seg_tail];

		i = checkpoint->seg_head; 
		while (i != checkpoint->seg_tail) {
			seg = &lfs->segments[i];

			log(LOG_INFO, "list item %i -> %i",
				i, checkpoint->segs[i]);

			seg->next = &lfs->segments[checkpoint->segs[i]];
		}

		lfs->seg_tail->next = nil;
	}

	log(LOG_INFO, "load free segs");

	lfs->seg_free = nil;
	f = nil;
	for (i = 0; i < lfs->n_segs; i++) {
		if (checkpoint->segs[i] != lfs_seg_free) continue;

		log(LOG_INFO, "seg %i is free", i);

		seg = &lfs->segments[i];

		if (f == nil) {
			lfs->seg_free = seg;
			f = seg;
		} else {
			f->next = seg;
			f = seg;
		}

		seg->next = nil;
	}

	return OK;
}

int
lfs_load_checkpoint(struct lfs *lfs)
{
	struct lfs_b_checkpoint *checkpoint;
	uint32_t timestamp[2];
	bool valid[2];
	size_t idx;

	log(LOG_INFO, "load checkpoints");

	checkpoint = lfs_load_checkpoint_data(lfs, 0);
	if (checkpoint != nil) {
		valid[0] = true;
		timestamp[0] = checkpoint->timestamp;
		unmap_addr(lfs->checkpoint_fid, checkpoint);
		log(LOG_INFO, "checkpoint %i has timestamp %i", 0, timestamp[0]);
	} else {
		valid[0] = false;
	}
	
	checkpoint = lfs_load_checkpoint_data(lfs, 1);
	if (checkpoint != nil) {
		valid[1] = true;
		timestamp[1] = checkpoint->timestamp;
		unmap_addr(lfs->checkpoint_fid, checkpoint);
		log(LOG_INFO, "checkpoint %i has timestamp %i", 1, timestamp[1]);
	} else {
		valid[1] = false;
	}

	if (valid[0] && valid[1]) {
		if (timestamp[0] == timestamp[1] + 1) {
			idx = 0;
		} else if (timestamp[1] == timestamp[0] + 1) {
			idx = 1;
		} else {
			log(LOG_FATAL, "checkpoint timestamps bad 0x%x and 0x%x",
				timestamp[0], timestamp[1]);
			return ERR;
		}
	} else if (valid[0]) {
		idx = 0;
	} else if (valid[1]) {
		idx = 1;
	} else {
		log(LOG_FATAL, "checkpoints both invalid!");
		return ERR;
	}

	checkpoint = lfs_load_checkpoint_data(lfs, idx);
	if (checkpoint == nil) {
		return ERR;
	}

	if (lfs_process_checkpoint(lfs, checkpoint)  != OK) {
		unmap_addr(lfs->checkpoint_fid, checkpoint);
		return ERR;
	}

	unmap_addr(lfs->checkpoint_fid, checkpoint);

	return OK;
}

int
lfs_write_superblock(struct lfs *lfs)
{
	struct lfs_b_superblock *super;
	int fid;

	fid = request_memory(lfs_blk_size, 0x1000);
	if (fid < 0) {
		return fid;
	}

	super = frame_map_anywhere(fid);
	if (super == nil) {
		release_memory(fid);
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

	unmap_addr(fid, super);

	if (block_write(lfs->blk_cid, fid, 
			lfs->part_off, lfs->sec_per_blk) != OK) 
	{
		release_memory(fid);
		return ERR;
	}
	
	release_memory(fid);

	return OK;
}

int
lfs_load_superblock(struct lfs *lfs)
{
	struct lfs_b_superblock *super;
	uint32_t crc_store, crc_expect;
	int fid;

	fid = request_memory(lfs_blk_size, 0x1000);
	if (fid < 0) {
		return fid;
	}

	if (block_read(lfs->blk_cid, fid, 
			lfs->part_off, lfs->sec_per_blk) != OK) 
	{
		release_memory(fid);
		return ERR;
	}
	
	super = frame_map_anywhere(fid);
	if (super == nil) {
		release_memory(fid);
		return ERR;
	}

	if (super->magic != lfs_superblock_magic) {
		log(LOG_FATAL, "superblock magic bad 0x%x != expected 0x%x",
			super->magic, lfs_superblock_magic);
		unmap_addr(fid, super);
		release_memory(fid);
		return ERR;
	}

	crc_store = super->crc;
	super->crc = 0;
	crc_expect = crc_checksum((uint8_t *) super, lfs_blk_size);
	if (crc_store != crc_expect) {
		log(LOG_FATAL, "superblock crc bad 0x%x != expected 0x%x",
			crc_store, crc_expect);
		unmap_addr(fid, super);
		release_memory(fid);
		return ERR;
	}

	lfs->n_segs = super->n_segs;
	lfs->n_inodes = super->n_inodes;
	lfs->checkpoint_blks = super->checkpoint_blks;
	lfs->checkpoint_blk[0] = super->checkpoint_blk[0];
	lfs->checkpoint_blk[1] = super->checkpoint_blk[1];
	lfs->seg_start = super->seg_start;

	log(LOG_INFO, "superblock fs has %i segs, %i inodes",
		lfs->n_segs, lfs->n_inodes);

	log(LOG_INFO, "superblock fs %i checkpoint blks at 0x%x and 0x%x",
		lfs->checkpoint_blks, 
		lfs->checkpoint_blk[0],
		lfs->checkpoint_blk[1]);

	log(LOG_INFO, "superblock fs segs start at 0x%x",
		lfs->seg_start);

	unmap_addr(fid, super);
	release_memory(fid);

	return OK;
}

int
lfs_scan_inode_blks(struct lfs *lfs, 
	struct lfs_inode *im, 
	struct lfs_b_inode *ib)
{
	struct lfs_blk *blk, *lblk;
	size_t b;

	log(LOG_INFO, "scan inode %i's blks (%i)", 
		im->index, im->blk_cnt);

	lblk = nil;

	for (b = 0; b < im->blk_cnt; b++) {
		blk = malloc(sizeof(struct lfs_blk));
		if (blk == nil) {
			return ERR;
		}

		blk->inode = im;
		blk->written = true;

		blk->off = b * lfs_blk_size;
		blk->cached = false;
		blk->mem_fid = -1;
		blk->addr = nil;
		blk->bnext = nil;

		blk->blk = ib->direct[b].blk;
		blk->crc = ib->direct[b].crc;
	
		if (blk->blk == 0 && blk->crc == 0) {
			blk->empty = true;
			log(LOG_INFO, "have empty blk for offset 0x%x", blk->off);
		} else {
			blk->empty = false;

			log(LOG_INFO, "have blk at 0x%x with crc 0x%x at offset 0x%x", 
				blk->blk, blk->crc, blk->off);
		}

		if (b == 0) {
			im->blks = blk;
		} else {
			lblk->bnext = blk;
		}

		lblk = blk;
	}

	/* TODO: indirect blks */

	return OK;
}

int
lfs_scan_inode(struct lfs *lfs, uint32_t blk)
{
	struct lfs_b_inode *inode;
	struct lfs_inode *im;
	uint32_t crc_store, crc_expect;
	int fid;

	log(LOG_INFO, "scan inode %i", blk);

	fid = request_memory(lfs_blk_size, 0x1000);
	if (fid < 0) {
		return ERR;
	}

	if (block_read(lfs->blk_cid, fid, 
			lfs->part_off 
			+ lfs->seg_start * lfs->sec_per_blk
			+ blk * lfs->sec_per_blk, 
			lfs->sec_per_blk) != OK) 
	{
		release_memory(fid);
		return ERR;
	}
	
	inode = frame_map_anywhere(fid);
	if (inode == nil) {
		release_memory(fid);
		return ERR;
	}

	if (inode->magic != lfs_inode_magic) {
		log(LOG_FATAL, "inode magic bad 0x%x != expected 0x%x",
			inode->magic, lfs_inode_magic);
		unmap_addr(fid, inode);
		release_memory(fid);
		return ERR;
	}

	crc_store = inode->crc;
	inode->crc = 0;
	crc_expect = crc_checksum((uint8_t *) inode, lfs_blk_size);
	if (crc_store != crc_expect) {
		log(LOG_FATAL, "inode crc bad 0x%x != expected 0x%x",
			crc_store, crc_expect);
		unmap_addr(fid, inode);
		release_memory(fid);
		return ERR;
	}

	im = &lfs->inodes[inode->index];

	im->created = true;
	im->free = false;

	memcpy(im->name, inode->name, lfs_name_len);
	im->blk_cnt = inode->blk_cnt;
	im->len_real = inode->len_real;

	im->inode_blk = blk;

	if (lfs_scan_inode_blks(lfs, im, inode) != OK) {
		unmap_addr(fid, inode);
		release_memory(fid);
		return ERR;
	}

	unmap_addr(fid, inode);
	release_memory(fid);

	log(LOG_INFO, "inode %i scanned", im->index);

	return OK;
}

int
lfs_scan_segment(struct lfs *lfs, struct lfs_segment *seg)
{
	struct lfs_b_segsum *segsum;
	uint32_t crc_store, crc_expect;
	int fid, i;

	log(LOG_INFO, "scan segment %i", seg->index);

	fid = request_memory(lfs_blk_size, 0x1000);
	if (fid < 0) {
		return ERR;
	}

	log(LOG_INFO, "lfs read segment %i from 0x%x + 0x%x*0x%x + 0x%x*0x%x*0x%x",
				seg->index, lfs->part_off, lfs->seg_start , lfs->sec_per_blk,
				seg->index , lfs_seg_blks , lfs->sec_per_blk);

	if (block_read(lfs->blk_cid, fid, 
			lfs->part_off 
			+ lfs->seg_start * lfs->sec_per_blk
			+ seg->index * lfs_seg_blks * lfs->sec_per_blk, 
			lfs->sec_per_blk) != OK) 
	{
		release_memory(fid);
		return ERR;
	}
	
	segsum = frame_map_anywhere(fid);
	if (segsum == nil) {
		release_memory(fid);
		return ERR;
	}

	if (segsum->magic != lfs_segsum_magic) {
		log(LOG_FATAL, "segsum magic bad 0x%x != expected 0x%x",
			segsum->magic, lfs_segsum_magic);
		unmap_addr(fid, segsum);
		release_memory(fid);
		return ERR;
	}

	crc_store = segsum->crc;
	segsum->crc = 0;
	crc_expect = crc_checksum((uint8_t *) segsum, lfs_blk_size);
	if (crc_store != crc_expect) {
		log(LOG_FATAL, "segsum crc bad 0x%x != expected 0x%x",
			crc_store, crc_expect);
		unmap_addr(fid, segsum);
		release_memory(fid);
		return ERR;
	}

	for (i = 0; i < segsum->blk_cnt; i++) {
		if (segsum->blk_inode[i] == lfs_blk_inode) {
			log(LOG_INFO, "seg %i blk %i is an inode",
				seg->index, i);

			if (lfs_scan_inode(lfs, seg->index * lfs_seg_blks + 1 + i) != OK) {
				unmap_addr(fid, segsum);
				release_memory(fid);

				return ERR;
			}
		}
	}

	/* TODO: try scan more incase the segsum didn't cover the
	   whole segment.
	   TODO: try scan next segment incase there was 
	   a crash and the checkpoint wasn't updated. */

	unmap_addr(fid, segsum);
	release_memory(fid);

	return OK;
}

int
lfs_scan(struct lfs *lfs)
{
	struct lfs_segment *seg;

	for (seg = lfs->seg_head; seg != nil; seg = seg->next) {
		if (lfs_scan_segment(lfs, seg) != OK) {
			return ERR;
		}
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

		if (f == nil) {
			lfs->seg_free = seg;
			f = seg;
		} else {
			f->next = seg;
			f = seg;
		}

		seg->next = nil;
	}

	struct lfs_inode *inode;
	for (i = 0; i < lfs->n_inodes; i++) {
		inode = &lfs->inodes[i];
		
		inode->index = i;
		inode->free = true;
		inode->created = false;

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
	if (lfs_load_superblock(lfs) != OK) {
		return ERR;
	}

	if (lfs_init(lfs) != OK) {
		return ERR;
	}

	if (lfs_load_checkpoint(lfs) != OK) {
		return ERR;
	}

	if (lfs_scan(lfs) != OK) {
		return ERR;
	}

	return OK;
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

	lfs->checkpoint_timestamp = 0;
	lfs->segsum_timestamp = 0;

	lfs->checkpoint_blk[0] = 1;
	lfs->checkpoint_blk[1] = lfs->checkpoint_blk[0] + lfs->checkpoint_blks;
	log(LOG_INFO, "put checkpoints at block %i and %i",
		lfs->checkpoint_blk[0], lfs->checkpoint_blk[1]);

	log(LOG_INFO, "put segments at block %i through %i",
		lfs->seg_start, lfs->seg_start + lfs->n_segs * lfs_seg_blks);

	log(LOG_INFO, "zero checkpoints");

	int zero_fid = request_memory(lfs_blk_size, 0x1000);
	if (zero_fid < 0) {
		return ERR;
	}

	log(LOG_INFO, "map zero");

	uint32_t *zero_va = frame_map_anywhere(zero_fid);
	if (zero_va == nil) {
		release_memory(zero_fid);
		return  ERR;
	}

	memset(zero_va, 0, lfs_blk_size);

	log(LOG_INFO, "unmap zero");
	unmap_addr(zero_fid, zero_va);

	size_t idx, o;
	for (idx = 0; idx < 2; idx++) {
		for (o = 0; o < lfs->checkpoint_blks; o++) {
			uint32_t blk = lfs->checkpoint_blk[idx] + o;
			
			log(LOG_INFO, "zero blk %i", blk);

			if (block_write(lfs->blk_cid, zero_fid,
					lfs->part_off + blk * lfs->sec_per_blk, 
					lfs->sec_per_blk) != OK) 
			{
				release_memory(zero_fid);
				return ERR;
			}
		}
	}

	release_memory(zero_fid);

	if (lfs_init(lfs) != OK) {
		return ERR;
	}

	if (lfs_write_superblock(lfs) != OK) {
		return ERR;
	}

	if (lfs_write_checkpoint(lfs) != OK) {
		return ERR;
	}

	return OK;
}

struct lfs_segment *
lfs_get_free_seg(struct lfs *lfs)
{
	struct lfs_segment *s;

	s = lfs->seg_free;
	lfs->seg_free = s->next;

	return s;
}

int
lfs_fill_inode(struct lfs *lfs,
	struct lfs_inode *inode,
	struct lfs_b_inode *buf)
{
	struct lfs_blk *blk;
	uint32_t blk_idx, blk_crc;
	size_t n;

	log(LOG_INFO, "fill inode %i for %i blks", 
		inode->index, inode->blk_cnt);

	buf->magic = lfs_inode_magic;
	buf->crc = 0;
	buf->index = inode->index;
	memcpy(buf->name, inode->name, lfs_name_len);
	buf->blk_cnt = inode->blk_cnt;
	buf->len_real = inode->len_real;

	n = 0;
	blk = inode->blks; 
	while (blk != nil) {
		log(LOG_INFO, "have block %i for offset 0x%x", n, (uint32_t) blk->off);

		if (blk->empty) {
			blk_idx = 0;
			blk_crc = 0;
		} else {
			blk_idx = blk->blk;
			blk_crc = blk->crc;
		}

		if (n < n_direct_blks) {
			log(LOG_INFO, "store direct blk %i for 0x%x,0x%x", n, blk_idx, blk_crc);
			buf->direct[n].blk = blk_idx;
			buf->direct[n].crc = blk_crc;
		} else {
			log(LOG_FATAL, "indirect blks not yet supported");
			return ERR;
		}

		log(LOG_INFO, "got to next");
	 	blk = blk->bnext;
	 	n++;
	}

	buf->crc = crc_checksum((uint8_t *) buf, lfs_blk_size);
	log(LOG_INFO, "inode has crc 0x%x", buf->crc);

	return OK;
}

int
lfs_fill_blk(struct lfs *lfs,
	struct lfs_blk *blk,
	uint8_t *buf,
	uint32_t blk_index)
{
	blk->crc = crc_checksum((uint8_t *) blk->addr, lfs_blk_size);
	blk->blk = blk_index;

	memcpy(buf, blk->addr, lfs_blk_size);

	return OK;
}

int
lfs_fill_seg(struct lfs *lfs, 
	struct lfs_segment *seg,
	struct lfs_b_segsum *s,
	size_t max_blks)
{
	struct lfs_inode *inode;
	struct lfs_blk *blk;

	log(LOG_INFO, "seg fill index %i", seg->index);

	s->magic = lfs_segsum_magic;
	s->crc = 0;
	s->timestamp = lfs->segsum_timestamp++;

	s->blk_cnt = 0;
	while (lfs->pending != nil) {
		if (s->blk_cnt == max_blks) {
			log(LOG_INFO, "end of segment");
			goto done;
		}

		inode = lfs->pending;
		
		log(LOG_INFO, "write inode %i blks", inode->index);

		for (blk = inode->blks; blk != nil; blk = blk->bnext) {
			log(LOG_INFO, "check blk 0x%x cached = %i, written = %i", 
				(uint32_t) blk->off, blk->cached, blk->written);

			if (s->blk_cnt == max_blks) {
				log(LOG_INFO, "end of segment");
				goto done;
			}

			if (!blk->written && !blk->empty) {
				log(LOG_INFO, "need to write block %i", blk->off);

				blk->crc = crc_checksum((uint8_t *) blk->addr, lfs_blk_size);
				blk->blk = seg->index * lfs_seg_blks + (1 + s->blk_cnt);
				
				log(LOG_INFO, "blk 0x%x with crc 0x%x going to 0x%x",
					(uint32_t) blk->off, blk->crc, blk->blk);

				memcpy(((uint8_t *) s) + (1 + s->blk_cnt) * lfs_blk_size, 
					blk->addr, lfs_blk_size);

				s->blk_inode[s->blk_cnt++] = inode->index;

				blk->written = true;
			}
		}

		if (s->blk_cnt == max_blks) {
			log(LOG_INFO, "end of segment");
			goto done;
		}

		log(LOG_INFO, "write inode %i", inode->index);

		if (lfs_fill_inode(lfs, inode, 
			(struct lfs_b_inode *) (((uint8_t *) s) 
			+ (s->blk_cnt + 1) * lfs_blk_size)) != OK) 
		{
			/* TODO: should set blks as not being written */
			return ERR;
		}

		s->blk_inode[s->blk_cnt++] = lfs_blk_inode;

		log(LOG_INFO, "move to next");

		lfs->pending = inode->wnext;
	}

done:	

	log(LOG_INFO, "seg fill done");

	s->crc = crc_checksum((uint8_t *) s, lfs_blk_size);
	
	log(LOG_INFO, "seg %i has crc 0x%x", seg->index, s->crc);

	return OK;
}

int
lfs_flush(struct lfs *lfs)
{
	struct lfs_b_segsum *s;
	struct lfs_segment *seg;
	int fid;

	log(LOG_INFO, "lfs flush");

	while (lfs->pending != nil) {
		log(LOG_INFO, "lfs build segment");

		seg = lfs_get_free_seg(lfs);
		if (seg == nil) {
			log(LOG_FATAL, "lfs get free failed");
			return ERR;
		}

		if (lfs->seg_tail == nil) {
			seg->index = 0;
		} else {
			seg->index = lfs->seg_tail->index + 1;
		}

		log(LOG_INFO, "seg index = %i", seg->index);
	
		fid = request_memory(lfs_seg_blks * lfs_blk_size, 0x1000);
		if (fid < 0) {
			log(LOG_FATAL, "lfs get seg mem failed");
			return fid;
		}

		s = frame_map_anywhere(fid);
		if (s == nil) {
			log(LOG_FATAL, "lfs map seg failed");
			release_memory(fid);
			return ERR;
		}

		log(LOG_INFO, "have seg mapped at 0x%x", s);

		if (lfs_fill_seg(lfs, seg, s, lfs_seg_blks-1) != OK) {
			log(LOG_FATAL, "lfs fill seg failed");
			unmap_addr(fid, s);
			release_memory(fid);
			return ERR;
		}

		size_t blk_cnt = s->blk_cnt;
				
		unmap_addr(fid, s);
			
		log(LOG_INFO, "lfs write segment %i to 0x%x + 0x%x*0x%x + 0x%x*0x%x*0x%x",
				seg->index, lfs->part_off, lfs->seg_start , lfs->sec_per_blk,
				seg->index , lfs_seg_blks , lfs->sec_per_blk);

		if (block_write(lfs->blk_cid, fid,
				lfs->part_off
				+ lfs->seg_start * lfs->sec_per_blk
				+ seg->index * lfs_seg_blks * lfs->sec_per_blk, 
				(1 + blk_cnt) * lfs->sec_per_blk) != OK) 
		{
			log(LOG_FATAL, "lfs write seg failed");
			release_memory(fid);
			return ERR;
		}
	
		log(LOG_INFO, "release segment");
		release_memory(fid);

		log(LOG_INFO, "put seg in list");
		if (lfs->seg_tail == nil) {
			log(LOG_INFO, "add seg to list as only");

			lfs->seg_head = seg;
			lfs->seg_tail = seg;
		} else {
			log(LOG_INFO, "add seg to list as tail");

			lfs->seg_tail->next = seg;
			lfs->seg_tail = seg;
		}
			
		seg->next = nil;
	}
	
	if (lfs_write_checkpoint(lfs) != OK) {
		return ERR;
	}

	log(LOG_INFO, "lfs flushed");

	return OK;
}

static uint32_t
lfs_hash(struct lfs *lfs, uint8_t name[lfs_name_len])
{
	uint32_t sum = 0;
	size_t i;

	for (i = 0; i < lfs_name_len; i++) {
		sum += name[i];
	}

	return sum % lfs->n_inodes;
}

static struct lfs_inode *
lfs_find_inode(struct lfs *lfs, 
	uint8_t name[lfs_name_len],
	bool creating)
{
	uint32_t h;
	struct lfs_inode *inode;
	uint32_t i;

	log(LOG_INFO, "find inode '%s'", name);

	h = lfs_hash(lfs, name);
	log(LOG_INFO, "start with hash %i", h);
	i = h;
	do {
		log(LOG_INFO, "check index %i", i);

		inode = &lfs->inodes[i];
		if (creating) {
			if (inode->free || !inode->created) {
				log(LOG_INFO, "creating and this is free or empty");
				return inode;
			}
		} else if (inode->free) {
			log(LOG_INFO, "inode is free");
			return nil;
		} else if (memcmp(inode->name, name, lfs_name_len)) {
			log(LOG_INFO, "name matches");
			return inode;
		}
		
		log(LOG_INFO, "hash colission with '%s'", inode->name);

		i = (i + 1) % lfs->n_inodes;
	} while (i != h);

	log(LOG_INFO, "nothing found");

	return nil;
}

static void
lfs_remove_pending(struct lfs *lfs, struct lfs_inode *inode)
{
	struct lfs_inode **o;

	for (o = &lfs->pending; *o != nil; o = &(*o)->wnext) {
		if (*o == inode) {
			*o = inode->wnext;
			break;
		} 
	}
}

static void
lfs_add_pending(struct lfs *lfs, struct lfs_inode *inode)
{
	lfs_remove_pending(lfs, inode);

	inode->wnext = lfs->pending;
	lfs->pending = inode;
}

int
lfs_create(struct lfs *lfs, uint8_t name[lfs_name_len])
{
	struct lfs_inode *inode;

	log(LOG_INFO, "lfs create '%s'", name);

	log(LOG_INFO, "check if it already exists");
	inode = lfs_find_inode(lfs, name, false);
	if (inode != nil) {
		log(LOG_INFO, "lfs create '%s' : already exists", name);
		return ERR;
	}

	log(LOG_INFO, "find spot for new");
	inode = lfs_find_inode(lfs, name, true);
	if (inode == nil) {
		log(LOG_INFO, "lfs create '%s' : nowhere found", name);
		return ERR;
	}

	log(LOG_INFO, "init new inode %i for '%s'", 
		inode->index, name);

	inode->created = true;
	inode->free = false;
	memcpy(inode->name, name, lfs_name_len);
	inode->blk_cnt = 0;
	inode->len_real = 0;
	inode->blks = nil;

	log(LOG_INFO, "inode name copied '%s'", inode->name);

	inode->written = false;
	inode->inode_blk = 0;

	lfs_add_pending(lfs, inode);

	return OK;
}

int
lfs_rm(struct lfs *lfs, uint8_t name[lfs_name_len])
{
	struct lfs_inode *inode;

	log(LOG_INFO, "lfs rm '%s'", name);

	inode = lfs_find_inode(lfs, name, false);
	if (inode != nil) {
		return ERR;
	}

	/* TODO: free blks */

	inode->blks = nil;

	inode->created = false;

	lfs_add_pending(lfs, inode);

	return ERR;
}

static struct lfs_blk *
lfs_create_blk(struct lfs_inode *inode, uint64_t off)
{
	struct lfs_blk *b;

	b = malloc(sizeof(struct lfs_blk));
	if (b == nil) {
		return nil;
	}

	b->inode = inode;

	b->written = false;
	b->blk = 0;
	b->crc = 0;

	b->off = off;

	b->cached = false;
	b->empty = true;

	return b;
}

static struct lfs_blk *
lfs_inode_get_blk(struct lfs *lfs, 
	struct lfs_inode *inode,
	uint64_t off, bool add)
{
	struct lfs_blk **b, *n;

	log(LOG_INFO, "inode %i find block 0x%x",
		inode->index, (uint32_t) off);

	if (align_down(off, (uint64_t) lfs_blk_size) != off) {
		log(LOG_FATAL, "inode get blk with unaligned off 0x%x", 
			off);
		return nil;
	}

	log(LOG_INFO, "past end?");
	if (!add && off > inode->blk_cnt * lfs_blk_size) {
		log(LOG_WARNING, "get blk 0x%x past max 0x%x",
			(uint32_t) off, inode->blk_cnt * lfs_blk_size);
		return nil;
	}

	log(LOG_INFO, "check through");

	b = &inode->blks;
	while (true) {
		log(LOG_INFO, "check 0x%x", *b);
		if (*b == nil) {
			log(LOG_INFO, "end of list, add? %i", add);
			if (add) {
				log(LOG_INFO, "add block offset 0x%x", (uint32_t) off);
				n = lfs_create_blk(inode, off);
				if (n == nil) {
					return nil;
				}

				inode->blk_cnt++;
				n->bnext = *b;
				*b = n;
			} else {
				log(LOG_INFO, "end of list");
				return nil;
			}
		}

		if (off == 0) {
			break;
		}

		log(LOG_INFO, "go to next");
		b = &(*b)->bnext;
		off -= lfs_blk_size;
	}

	return *b;
}

static int
lfs_cache_blk(struct lfs *lfs, struct lfs_blk *blk)
{
	uint32_t crc;

	log(LOG_INFO, "cache inode %i block offset 0x%x",
		blk->inode->index, (uint32_t) blk->off);

	if (blk->cached) {
		return OK;
	}

	blk->mem_fid = request_memory(lfs_blk_size, 0x1000);
	if (blk->mem_fid < 0) {
		return blk->mem_fid;
	}

	if (!blk->empty) {
		log(LOG_INFO, "not empty, read blk 0x%x",
			blk->blk);

		if (block_read(lfs->blk_cid, blk->mem_fid, 
			lfs->part_off
			+ (lfs->seg_start + blk->blk) * lfs->sec_per_blk,
			lfs->sec_per_blk) != OK)
		{
			release_memory(blk->mem_fid);
			blk->mem_fid = -1;
			return ERR;
		}
	}
	
	blk->addr = frame_map_anywhere(blk->mem_fid);
	if (blk->addr == nil) {
		release_memory(blk->mem_fid);
		blk->mem_fid = -1;
		return ERR;
	}

	if (!blk->empty) {
		crc = crc_checksum(blk->addr, lfs_blk_size);
		if (crc != blk->crc) {
			log(LOG_WARNING, "inode %i blk 0x%x crc mismatch 0x%x != expected 0x%x",
				blk->inode->index, (uint32_t) blk->off,
				crc, blk->crc);

			unmap_addr(blk->mem_fid, blk->addr);
			release_memory(blk->mem_fid);
			blk->addr = nil;
			blk->mem_fid = -1;
			return nil;
		}
	} else {
		log(LOG_INFO, "empty block, memset buffer");
		memset(blk->addr, 0, lfs_blk_size);
	}

	blk->cached = true;

	return OK;
}

int
lfs_write(struct lfs *lfs, uint8_t name[lfs_name_len],
	void *bbuf, uint64_t off, uint32_t len)
{
	uint8_t *buf = bbuf;
	struct lfs_blk *b;
	uint64_t aligned_off, b_off;
	struct lfs_inode *inode;
	uint32_t n;

	log(LOG_INFO, "lfs write '%s'", name);

	inode = lfs_find_inode(lfs, name, false);
	if (inode == nil) {
		log(LOG_WARNING, "didn't find inode");
		return ERR;
	}

	aligned_off = align_down(off, (uint64_t) lfs_blk_size);
	b_off = off - aligned_off;

	b = lfs_inode_get_blk(lfs, inode, aligned_off, true);
	if (b == nil) {
		log(LOG_WARNING, "block not found and wasn't created");
		return ERR;
	}

	while (len > 0) {
		log(LOG_INFO, "lfs write off %i", (uint32_t) off);

		if (b == nil) {
			log(LOG_INFO, "no blk found");
			return ERR;
		}

		log(LOG_INFO, "cache blk");

		if (lfs_cache_blk(lfs, b) != OK) {
			return ERR;
		}

		if (b_off + len > lfs_blk_size) {
			n = lfs_blk_size - b_off;
		} else {
			n = len;
		}
		
		log(LOG_INFO, "write %i bytes to offset %i in block", n, b_off);
		
		b->empty = false;
		b->written = false;

		memcpy(b->addr + b_off, buf, n);
		
		buf += n;
		off += n;
		len -= n;
		b_off = 0;

		if (b->bnext == nil && len > 0) {
			log(LOG_INFO, "create next blk");

			b->bnext = lfs_create_blk(inode, off);
			if (b->bnext != nil) {
				inode->blk_cnt++;
			}
		}

		b = b->bnext;
	}

	if (inode->len_real < off + len) {
		inode->len_real = off + len;
	}

	log(LOG_INFO, "cached write finished");

	lfs_add_pending(lfs, inode);

	return OK;
}

int
lfs_read(struct lfs *lfs, uint8_t name[lfs_name_len],
	void *bbuf, uint64_t off, uint32_t len)
{
	uint8_t *buf = bbuf;
	struct lfs_blk *b;
	uint64_t aligned_off, b_off;
	struct lfs_inode *inode;
	uint32_t n;

	log(LOG_INFO, "lfs read '%s'", name);

	inode = lfs_find_inode(lfs, name, false);
	if (inode == nil) {
		log(LOG_WARNING, "didn't find inode");
		return ERR;
	}

	if (inode->len_real < off + len) {
		log(LOG_WARNING, "trying to read past end");
		return ERR;
	}

	aligned_off = align_down(off, (uint64_t) lfs_blk_size);
	b_off = off - aligned_off;

	b = lfs_inode_get_blk(lfs, inode, aligned_off, false);
	if (b == nil) {
		log(LOG_INFO, "block not found");
		return ERR;
	}

	while (len > 0) {
		log(LOG_INFO, "lfs read off %i", (uint32_t) off);

		if (b == nil) {
			log(LOG_INFO, "no blk found");
			return ERR;
		}

		log(LOG_INFO, "cache blk");

		if (lfs_cache_blk(lfs, b) != OK) {
			return ERR;
		}

		if (b_off + len > lfs_blk_size) {
			n = lfs_blk_size - b_off;
		} else {
			n = len;
		}
		
		log(LOG_INFO, "read %i bytes at offset %i in block", n, b_off);
		
		memcpy(buf, b->addr + b_off, n);
		
		buf += n;
		off += n;
		len -= n;
		b_off = 0;

		b = b->bnext;
	}

	log(LOG_INFO, "read finished");

	return OK;
}

int
lfs_trunc(struct lfs *lfs, uint8_t name[lfs_name_len],
	uint64_t len)
{
	struct lfs_inode *inode;
	struct lfs_blk **b;
	size_t n;

	log(LOG_INFO, "lfs trunc '%s'", name);

	inode = lfs_find_inode(lfs, name, false);
	if (inode == nil) {
		log(LOG_WARNING, "didn't find inode");
		return ERR;
	}

	if (len > inode->len_real) {
		log(LOG_WARNING, "trying to trunc to larger than current");
		return ERR;
	} else if (len == inode->len_real) {
		return OK;
	}

	inode->len_real = len;

	n = 0;
	b = &inode->blks;
	while (*b != nil) {
		if ((*b)->off >= len) {
			*b = nil;
			break;
			/* TODO: free stuff*/
		}

		b = &(*b)->bnext;
		n++;
	}

	inode->blk_cnt = n;
	
	lfs_add_pending(lfs, inode);

	return OK;
}

