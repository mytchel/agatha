/* On Block structures */

#define lfs_blk_size   4096
#define lfs_name_len     16

struct lfs_b_blk_info {
	uint32_t blk;
	uint32_t crc;
};

#define lfs_inode_magic   0xbf87ca93
struct lfs_b_inode {
	uint32_t magic;
	uint32_t crc;

	uint32_t index;
	uint8_t name[lfs_name_len];

	uint32_t blk_cnt;
	uint64_t len_real;

#define n_direct_blks   16
	struct lfs_b_blk_info direct[n_direct_blks];
	struct lfs_b_blk_info indirect;
	struct lfs_b_blk_info double_indirect;
};

#define lfs_seg_blks  256

/* If we put the next segment here rather than
   store the list in the checkpoint we can
   recover more easily. But that makes cleaning
   anything but the head impossible. Could still
   put the next segment here as well as using
   the checkpoint. Will need a timestamp to prove
   that the segment is newer and should be appending
   to the log. */

#define lfs_segsum_magic   0xbf87ca92
struct lfs_b_segsum {
	uint32_t magic;
	uint32_t crc;

	uint32_t timestamp;

#define lfs_blk_inode   0xffffffff

	uint32_t blk_cnt;
	uint32_t blk_inode[255];
};

#define lfs_checkpoint_magic   0xbf87ca91
struct lfs_b_checkpoint {
	uint32_t magic;
	uint32_t crc;

	uint32_t timestamp;

	uint32_t seg_head;
	uint32_t seg_tail;
#define lfs_seg_free   0xffffffff
#define lfs_seg_end    0xfffffffe
	uint32_t segs[];
};

#define lfs_superblock_magic   0xbf87ca90
struct lfs_b_superblock {
	uint32_t magic;
	uint32_t crc;

	uint32_t n_segs;
	uint32_t n_inodes;
	uint32_t checkpoint_blks;

	uint32_t checkpoint_blk[2];
	uint32_t seg_start;
};

/* In memory structures */

struct lfs_blk {
	struct lfs_inode *inode;

	bool written;
	uint32_t blk;
	uint32_t crc;

	uint64_t off;

	bool empty;

	bool cached;
	int mem_fid;
	uint8_t *addr;

	struct lfs_blk *bnext;
};

struct lfs_inode {
	uint32_t index;

	bool created;
	bool free;

	char name[lfs_name_len];
	uint32_t blk_cnt;
	uint64_t len_real;
	
	bool written;
	uint32_t inode_blk;

	struct lfs_blk *blks;

	struct lfs_inode *wnext;
};

struct lfs_segment {
	uint32_t index;
	struct lfs_segment *next;
};

struct lfs {
	int blk_cid;
	size_t part_off, part_len;
	size_t sec_size;
	size_t sec_per_blk;

	int checkpoint_fid;
	uint32_t checkpoint_blk[2];
	uint32_t checkpoint_blks;

	uint32_t seg_start;
	
	uint32_t checkpoint_timestamp;
	uint32_t segsum_timestamp;

	size_t n_segs;
	struct lfs_segment *segments;
	struct lfs_segment *seg_head;
	struct lfs_segment *seg_tail;
	struct lfs_segment *seg_free;

	size_t n_inodes;
	struct lfs_inode *inodes;
	struct lfs_inode *pending;
};

int
lfs_init(struct lfs *lfs);

void
lfs_free(struct lfs *lfs);

int
lfs_setup(struct lfs *lfs,
	int blk_cid, size_t blk_off, size_t blk_len);

int
lfs_load(struct lfs *lfs);

int
lfs_format(struct lfs *lfs);

int
lfs_flush(struct lfs *lfs);

int
lfs_create(struct lfs *lfs, uint8_t name[lfs_name_len]);

int
lfs_rm(struct lfs *lfs, uint8_t name[lfs_name_len]);

int
lfs_write(struct lfs *lfs, uint8_t name[lfs_name_len],
	void *buf, uint64_t off, uint32_t len);

int
lfs_read(struct lfs *lfs, uint8_t name[lfs_name_len],
	void *buf, uint64_t off, uint32_t len);

int
lfs_trunc(struct lfs *lfs, uint8_t name[lfs_name_len],
	uint64_t len);

/* Helpers */

int
block_read(int cid, int fid, size_t start_blk, size_t len);

int
block_write(int cid, int fid, size_t start_blk, size_t len);

int
block_get_block_size(int cid, size_t *blk_size);

