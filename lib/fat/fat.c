#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <proc0.h>
#include <mmu.h>
#include <stdarg.h>
#include <string.h>
#include <log.h>
#include <block.h>
#include <fat.h>

	int
fat_init(struct fat *fat, int block_eid, 
		size_t block_size, size_t p_start, size_t p_size)
{
	struct fat_bs *bs;
	int ret, fid;
	size_t len;

	memset(fat, 0, sizeof(struct fat));

	fat->block_eid = block_eid;
	fat->block_size = block_size;

	fat->start = p_start;
	fat->nblocks = p_size;

	len = PAGE_ALIGN(fat->block_size);
	fid = request_memory(len, 0x1000);
	if (fid < 0) {
		log(LOG_INFO, "fat_read_bs, mem req failed");
		return ERR;
	}

	log(LOG_INFO, "read bs block 0x%x", fat->start);
	
	ret = fat_read_blocks(fat, fid, 0, 0, fat->block_size);
	if (ret != OK) {
		log(LOG_INFO, "fat_read_bs, read blocks failed");
		return ret;
	}

	bs = frame_map_anywhere(fid);
	if (bs == nil) {
		log(LOG_INFO, "map_addr failed");
		return ERR;
	}

	fat->bps = intcopylittle16(bs->bps);
	fat->spc = bs->spc;

	fat->nsectors = intcopylittle16(bs->sc16);
	if (fat->nsectors == 0) {
		log(LOG_INFO, "nsectors zero use 32");
		fat->nsectors = intcopylittle32(bs->sc32);
	}

	log(LOG_INFO, "fat bps %i spc %i nsectors %i",
			fat->bps, fat->spc, fat->nsectors);

	fat->nft = bs->nft;

	fat->reserved = intcopylittle16(bs->res);

	fat->rde = intcopylittle16(bs->rde);
	if (fat->rde != 0) {
		log(LOG_INFO, "fat type FAT16");

	    fat->type = FAT16;

		fat->spf = intcopylittle16(bs->spf);

	    fat->rootdir = fat->reserved + fat->nft * fat->spf;
	    fat->dataarea = fat->rootdir +
	      (((fat->rde * sizeof(struct fat_dir_entry)) + (fat->bps - 1))
	       / fat->bps);

	    fat->files[FILE_root_fid].dsize = fat->rde * sizeof(struct fat_dir_entry);
	    fat->files[FILE_root_fid].start_cluster = 0;

	} else {
		log(LOG_INFO, "fat type FAT32");
	    fat->type = FAT32;

			fat->spf = intcopylittle32(bs->ext.high.spf);

	    fat->dataarea = fat->reserved + fat->spf * fat->nft;

	    /* This should work for now */

	    fat->files[FILE_root_fid].dsize = fat->spc * fat->bps;
	    fat->files[FILE_root_fid].start_cluster =
				intcopylittle32(bs->ext.high.rootcluster);
	}

	log(LOG_INFO, "root dsize = 0x%x, start cluster= 0x%x",
			fat->files[0].dsize, fat->files[0].start_cluster);
	log(LOG_INFO, "root dir = 0x%x", fat->rootdir);

	fat->nclusters = (fat->nsectors - fat->dataarea) / fat->spc;

	fat->files[FILE_root_fid].refs = 1;
	fat->files[FILE_root_fid].size = 0;

	unmap_addr(fid, bs);
	release_memory(fid);

	return OK;
}

	static int
fat_find_file_in_sectors(struct fat *fat, struct fat_dir_entry *e,
		uint32_t sector, size_t nsectors,
	 	char *name)
{
	struct fat_dir_entry *files;
	size_t len, nfiles;
	char fname[32];
	int r, i, fid;

	log(LOG_INFO, "find file %s in sectors 0x%x 0x%x", name, sector, nsectors);

	len = PAGE_ALIGN(nsectors * fat->bps);
	fid = request_memory(len, 0x1000);
	if (fid < 0) {
		return -1;
	}

	r = fat_read_blocks(fat, 
			fid, 0,
			sector * fat->bps, 
			nsectors * fat->bps);
	if (r != OK) {
		log(LOG_INFO, "read failed");
		release_memory(fid);
		return -1;
	}

	nfiles = nsectors * fat->bps / sizeof(struct fat_dir_entry);

	files = frame_map_anywhere(fid);
	if (files == nil) {
		release_memory(fid);
		return -1;
	}

	for (i = 0; i < nfiles; i++) {
		r = fat_copy_file_entry_name(&files[i], fname);
		log(LOG_INFO, "copied name got %i, '%s'", r, fname);

		if (r == -3) {
			/* Skip this for some reason */
			continue;

		} else if (r == -2) {
			/* For now, skip lfn errors */
			continue;

		} else if (r != OK) {
			log(LOG_WARNING, "bad");
			unmap_addr(fid, files);
			release_memory(fid);
			return -1;

		} else if (strncmp(fname, name, 32)) {
			log(LOG_INFO, "found");
			break;
		}
	}

	if (i < nfiles) {
		log(LOG_INFO, "copy struct");
		memcpy(e, &files[i], sizeof(struct fat_dir_entry));
	}
	
	log(LOG_INFO, "done");

	unmap_addr(fid, files);
	release_memory(fid);

	return i < nfiles ? 1 : 0;
}

	int
fat_file_find(struct fat *fat, 
		struct fat_file *parent,
		char *name)
{
	struct fat_dir_entry e;
	uint32_t cluster;
	int r, i;

	log(LOG_INFO, "finding file %s", name);

	for (i = 1; i < FIDSMAX; i++) {
		if (fat->files[i].parent != parent) continue;
		if (fat->files[i].refs != 0) continue;
		if (strncmp(fat->files[i].name, name, 32)) {
			/* Slot was clunked but still in array */
			return i;
		}
	}

	if (parent->start_cluster == 0) {
		r = fat_find_file_in_sectors(fat, &e,
				fat->rootdir, fat->spc, name);
	} else {
		cluster = parent->start_cluster;
		while (cluster != 0) {
			r = fat_find_file_in_sectors(fat, &e,
					cluster_to_sector(fat, cluster), 
					fat->spc, name);

			if (r	== 0) {
				cluster = fat_next_cluster(fat, cluster);
			} else {
				break;
			}
		}
	}

	log(LOG_INFO, "r = %i", r);

	if (r != 1) {
		return ERR;
	}

	return fat_file_from_entry(fat, &e, name, parent);
}

	int
fat_file_read(struct fat *fat, 
		struct fat_file *file,
		int fid, 
		size_t frame_offset,
		size_t file_offset, 
		size_t len)
{
	uint32_t cluster, tlen;
	int ret;

	if (file_offset % (fat->spc * fat->bps) != 0) {
		log(LOG_WARNING, "fat_file_read can only read %i aligned from file",
			fat->spc * fat->bps);
		return ERR;
	}

	tlen = 0;
	for (cluster = file->start_cluster;
			cluster != 0 && tlen < len;
			cluster = fat_next_cluster(fat, cluster)) 
	{
		if (file_offset > 0) {
			file_offset -= fat->spc * fat->bps;
			continue;
		}

		/* TODO: this needs to be made to work with alignements */
		ret = fat_read_blocks(fat, 
				fid, frame_offset + tlen, 
				cluster_to_sector(fat, cluster) * fat->bps, 
				fat->spc * fat->bps);
		if (ret != OK) {
			return ret;
		}

		tlen += fat->spc * fat->bps;
	}

	return OK;
}


	void
fat_file_clunk(struct fat *fat, struct fat_file *f)
{
	if (f->refs == 0) {
		/* File removed now wipe everything */
		memset(f, 0, sizeof(struct fat_file));
	} else {
		f->refs = 0;
	}
}

	uint32_t
fat_next_cluster(struct fat *fat, uint32_t prev)
{
	uint32_t v;

	v = fat_table_info(fat, prev);

	switch (fat->type) {
		case FAT16:
			v &= 0xffff;
			if (v >= 0xfff8) {
				v = 0;
			}
			break;
		case FAT32:
			v &= 0x0fffffff;
			if (v >= 0x0ffffff8) {
				v = 0;
			}
			break;
	}

	return v;
}

	uint32_t
fat_table_info(struct fat *fat, uint32_t cluster)
{
	uint32_t sec, off, v;
	size_t len;
	uint8_t *va;
	int r, fid;

	switch (fat->type) {
		case FAT16:
			off = cluster * sizeof(uint16_t);
			break;
		case FAT32:
			off = cluster * sizeof(uint32_t);
			break;
		default:
			return 0;
	}

	sec = off / fat->bps;

	len = PAGE_ALIGN(fat->bps);
	fid = request_memory(len, 0x1000);
	if (fid < 0) {
		return 0;
	}

	r = fat_read_blocks(fat, fid, 0,
			(fat->reserved + sec) * fat->bps, 
			fat->bps);

	if (r != OK) {
		release_memory(fid);
		return 0;
	}

	va = frame_map_anywhere(fid);
	if (va == nil) {
		release_memory(fid);
		return 0;
	}

	off = off % fat->bps;

	switch (fat->type) {
		case FAT16:
			v = intcopylittle16(va + off);
			break;
		case FAT32:
			v = intcopylittle32(va + off);
			break;
	}

	unmap_addr(fid, va);
	release_memory(fid);

	return v;
}

	int	
fat_copy_file_entry_name(struct fat_dir_entry *file, char *name)
{
	size_t i, j;

	if (file->name[0] == 0) {
		log(LOG_INFO, "empty");
		return ERR;
	} else if (file->name[0] == 0xe5) {
		return -3;
	}

	while ((file->attr & FAT_ATTR_lfn) == FAT_ATTR_lfn) {
		log(LOG_INFO, "lfn entry, not sure what to do");
		return -2;
	} 

	i = 0;

	for (j = 0; j < sizeof(file->name) && file->name[j] != ' '; j++) {
		if (file->name[j] >= 'A' && file->name[j] <= 'Z') {
			/* Convert upper to lower */
			name[i++] = file->name[j] + 32;
		} else {
			name[i++] = file->name[j];
		}
	}

	if (file->ext[0] != ' ') {
		name[i++] = '.';

		for (j = 0; j < sizeof(file->ext) && file->ext[j] != ' '; j++) {
			if (file->ext[j] >= 'A' && file->ext[j] <= 'Z') {
				/* Convert upper to lower */
				name[i++] = file->ext[j] + 32;
			} else {
				name[i++] = file->ext[j];
			}
		}
	}

	name[i] = 0;

	return OK;
}

	uint32_t
fat_find_free_fid(struct fat *fat)
{
	int i, b;

	/* Skip root fid and search for empty slot */
	b = 0;
	for (i = 1; i < FIDSMAX; i++) {
		fat->files[i].name[sizeof(fat->files[i].name)-1] = 0;
		log(LOG_INFO, "fat fid table %i = '%s'", i, fat->files[i].name);
		if (fat->files[i].name[0] == 0) {
			/* Slot completely unused */
			b = i;
			break;
		} else if (b == 0 && fat->files[i].refs == 0) {
			/* Slot clunked but could be reused */
			b = i;
		}
	}

	if (b == 0) {
		log(LOG_INFO, "fat mount fid table full!");
		return 0;
	} else {
		memset(&fat->files[b], 0, sizeof(struct fat_file));
	}

	return b;
}

int
fat_file_from_entry(struct fat *fat, 
		struct fat_dir_entry *entry,
		char *name, struct fat_file *parent)
{
	struct fat_file *f;
	int i;
	i = fat_find_free_fid(fat);
	if (i == 0) {
		return ERR;
	}

	f = &fat->files[i];

	strlcpy(f->name, name, 32);

	f->refs = 1;
	f->parent = parent;

	f->size = intcopylittle32(entry->size);

	f->start_cluster =
		((uint32_t) intcopylittle16(entry->cluster_high)) << 16;

	f->start_cluster |=
		((uint32_t) intcopylittle16(entry->cluster_low));

	if (entry->attr & FAT_ATTR_read_only) {
		f->attr = FILE_ATTR_rd;
	} else {
		f->attr = FILE_ATTR_rd | FILE_ATTR_wr;
	}

	if (entry->attr & FAT_ATTR_directory) {
		f->attr |= FILE_ATTR_dir;
		f->dsize = fat->spc * fat->bps;
		f->size = 0;
	} else {
		f->dsize = fat_file_size(fat, f);
	}

	return i;
}

	uint32_t
fat_file_size(struct fat *fat, struct fat_file *file)
{
	uint32_t cluster, s;

	s = 0;
	if (file->start_cluster == 0) {
		s = fat->bps * fat->spc;
	} else {
		cluster = file->start_cluster;

		while (cluster != 0) {
			s += fat->bps * fat->spc;
			cluster = fat_next_cluster(fat, cluster);
		}
	}

	return s;
}

	int
fat_read_blocks(struct fat *fat, 
		int fid, size_t off,
		size_t start, size_t len)
{
	union block_req rq;
	union block_rsp rp;

	rq.read.type = BLOCK_read;
	rq.read.start = fat->start * fat->block_size + start;
	rq.read.len = len;
	rq.read.off = off;
	
	log(LOG_INFO, "block sending request for 0x%x 0x%x", 
		rq.read.start, len);
	
	if (mesg_cap(fat->block_eid, &rq, &rp, fid) != OK) {
		log(LOG_INFO, "block mesg failed");
		return ERR;
	} else if (rp.read.ret != OK) {
		log(LOG_INFO, "block read returned bad %i", rp.read.ret);
		return ERR;
	}

	return OK;
}


