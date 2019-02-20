#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <dev_reg.h>
#include <block_dev.h>
#include <fs.h>
#include <fat.h>

int fat_debug_pid = 3;

static void
fat_debug(char *fmt, ...)
{
	char m[MESSAGE_LEN] = "fat: ";
	va_list a;

	if (fat_debug_pid == 0) 
		return;

	va_start(a, fmt);
	vsnprintf(m + strlen(m), 
			sizeof(m) - strlen(m), 
			fmt, a);
	va_end(a);

	send(fat_debug_pid, m);
	recv(fat_debug_pid, m);
}

	int
fat_init(struct fat *fat, int block_pid, 
		size_t block_size, size_t p_start, size_t p_size)
{
  struct fat_bs *bs;
	size_t pa, len;
	int ret;

	memset(fat, 0, sizeof(struct fat));

	fat->block_pid = block_pid;
	fat->block_size = block_size;

	fat->start = p_start;
	fat->nblocks = p_size;

	len = PAGE_ALIGN(fat->block_size);
	pa = request_memory(len);
	if (pa == nil) {
		fat_debug("fat_read_bs, mem req failed\n");
		return ERR;
	}

	fat_debug("reading bs from block 0x%x into 0x%x, 0x%x\n", fat->start, pa, len);
	
	ret = fat_read_blocks(fat, pa, len, 0, fat->block_size);

	if (ret != OK) {
		fat_debug("fat_read_bs, read blocks failed\n");
		return ret;
	}

	bs = map_addr(pa, len, MAP_RO);
	if (bs == nil) {
		fat_debug("map_addr failed\n");
		return ERR;
	}

  fat->bps = intcopylittle16(bs->bps);
  fat->spc = bs->spc;

	fat->nsectors = intcopylittle16(bs->sc16);
  if (fat->nsectors == 0) {
		fat_debug("nsectors zero use 32\n");
		fat->nsectors = intcopylittle32(bs->sc32);
  }

	fat_debug("fat bps = %i, spc = %i, nsectors = %i\n",
			fat->bps, fat->spc, fat->nsectors);

  fat->nft = bs->nft;

  fat->reserved = intcopylittle16(bs->res);

  fat->rde = intcopylittle16(bs->rde);
  if (fat->rde != 0) {
		fat_debug("fat type FAT16\n");

    fat->type = FAT16;

		fat->spf = intcopylittle16(bs->spf);

    fat->rootdir = fat->reserved + fat->nft * fat->spf;
    fat->dataarea = fat->rootdir +
      (((fat->rde * sizeof(struct fat_dir_entry)) + (fat->bps - 1))
       / fat->bps);

    fat->files[FILE_root_fid].dsize = fat->rde * sizeof(struct fat_dir_entry);
    fat->files[FILE_root_fid].start_cluster = 0;

  } else {
		fat_debug("fat type FAT32\n");
    fat->type = FAT32;

		fat->spf = intcopylittle32(bs->ext.high.spf);

    fat->dataarea = fat->reserved + fat->spf * fat->nft;

    /* This should work for now */

    fat->files[FILE_root_fid].dsize = fat->spc * fat->bps;
    fat->files[FILE_root_fid].start_cluster =
			intcopylittle32(bs->ext.high.rootcluster);
	}

	fat_debug("root dir dsize = 0x%x, start cluster= 0x%x\n",
			fat->files[0].dsize, fat->files[0].start_cluster);
	fat_debug("root dir = 0x%x\n", fat->rootdir);

	fat->nclusters = (fat->nsectors - fat->dataarea) / fat->spc;

	fat->files[FILE_root_fid].refs = 1;
	fat->files[FILE_root_fid].size = 0;

	unmap_addr(bs, len);
	release_addr(pa, len);

	return OK;
}

	static int
fat_find_file_in_sectors(struct fat *fat, struct fat_dir_entry *e,
		uint32_t sector, size_t nsectors,
	 	char *name)
{
	struct fat_dir_entry *files;
	size_t pa, len, nfiles;
	char fname[32];
	int r, i;

	fat_debug("find file %s in sectors 0x%x 0x%x\n", name, sector, nsectors);

	len = PAGE_ALIGN(nsectors * fat->bps);
	pa = request_memory(len);
	if (pa == nil) {
		return -1;
	}

	r = fat_read_blocks(fat, pa, len, 
			sector * fat->bps, nsectors * fat->bps);
	if (r != OK) {
		fat_debug("read failed\n");
		return -1;
	}

	nfiles = nsectors * fat->bps / sizeof(struct fat_dir_entry);

	files = map_addr(pa, len, MAP_RO);
	for (i = 0; i < nfiles; i++) {
		r = fat_copy_file_entry_name(&files[i], fname);
		fat_debug("copied name got %i, '%s'\n", r, fname);
		if (r == -3) {
			/* Skip this for some reason */
			continue;
		} else if (r == -2) {
			/* For now, skip lfn errors */
			continue;
		} else if (r != OK) {
			fat_debug("bad\n");
			unmap_addr(files, len);
			release_addr(pa, len);
			return -1;
		} else if (strncmp(fname, name, 32)) {
			fat_debug("found\n");
			break;
		}
	}

	if (i < nfiles) {
		fat_debug("copy struct\n");
		memcpy(e, &files[i], sizeof(struct fat_dir_entry));
	}
	
	fat_debug("done\n");

	unmap_addr(files, len);
	release_addr(pa, len);

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

	fat_debug("finding file %s\n", name);

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

	fat_debug("r = %i\n", r);

	if (r != 1) {
		return ERR;
	}

	return fat_file_from_entry(fat, &e, name, parent);
}

	int
fat_file_read(struct fat *fat, struct fat_file *file,
		size_t pa, size_t m_len,
		size_t offset, size_t r_len)
{
	uint32_t cluster, tlen;
	int ret;

	tlen = 0;
	for (cluster = file->start_cluster;
			cluster != 0 && tlen < r_len;
			cluster = fat_next_cluster(fat, cluster)) {

		if (offset > 0) {
			offset -= fat->spc * fat->bps;
			continue;
		}

		/* TODO: this needs to be made to work with alignements */
		ret = fat_read_blocks(fat, pa + tlen, PAGE_ALIGN(fat->spc * fat->bps),
				cluster_to_sector(fat, cluster) * fat->bps, fat->spc * fat->bps);
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
	size_t pa, len;
	uint8_t *va;
	int r;

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
	pa = request_memory(len);
	if (pa == nil) {
		return 0;
	}

	r = fat_read_blocks(fat, pa, len,
			(fat->reserved + sec) * fat->bps, fat->bps);
	if (r != OK) {
		release_addr(pa, len);
		return 0;
	}

	va = map_addr(pa, len, MAP_RO);

	off = off % fat->bps;

	switch (fat->type) {
		case FAT16:
			v = intcopylittle16(va + off);
			break;
		case FAT32:
			v = intcopylittle32(va + off);
			break;
	}

	unmap_addr(va, len);
	release_addr(pa, len);

	return v;
}

	int	
fat_copy_file_entry_name(struct fat_dir_entry *file, char *name)
{
	size_t i, j;

	if (file->name[0] == 0) {
		fat_debug("empty\n");
		return ERR;
	} else if (file->name[0] == 0xe5) {
		return -3;
	}

	while ((file->attr & FAT_ATTR_lfn) == FAT_ATTR_lfn) {
		fat_debug("This is a long file name entry, not sure what to do\n");
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
		fat_debug("fat fid table %i = '%s'\n", i, fat->files[i].name);
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
		fat_debug("fat mount fid table full!\n");
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
fat_read_blocks(struct fat *fat, size_t pa, size_t len,
		size_t start, size_t r_len)
{
	union block_dev_req rq;
	union block_dev_rsp rp;
	int ret;

	fat_debug("give block pid %i from 0x%x 0x%x\n", fat->block_pid, pa, len);
	if ((ret = give_addr(fat->block_pid, pa, len)) != OK) {
		fat_debug("block give addr failed %i\n", ret);
		return ERR;
	}

	rq.read.type = BLOCK_DEV_read;
	rq.read.pa = pa;
	rq.read.len = len;
	rq.read.start = fat->start * fat->block_size + start;
	rq.read.r_len = r_len;
	
	fat_debug("block sending request for 0x%x 0x%x\n", rq.read.start, r_len);
	
	if (mesg(fat->block_pid, &rq, &rp) != OK) {
		fat_debug("block mesg failed\n");
		return ERR;
	} else if (rp.read.ret != OK) {
		fat_debug("block read returned bad %i\n", rp.read.ret);
		return ERR;
	}

	fat_debug("block read responded good, map\n");
	return OK;
}


