/* On Block structures */

#define lfs_blk_size 4096

struct lfs_b_blk_info {
	uint32_t blk;
	uint32_t crc;
};

#define lfs_inode_magic   0xbf87ca93
struct lfs_b_inode {
	uint32_t magic;
	uint32_t crc;

	uint32_t inode_idx;
	uint8_t name[16];

	uint32_t blk_cnt;
	uint64_t len_real;

#define n_direct_blks 16
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

#define lfs_seg_magic   0xbf87ca92
struct lfs_b_seg_sum {
	uint32_t magic;
	uint32_t crc;

#define lfs_blk_free    0
#define lfs_blk_inode   1

	uint32_t blk_inode[255];
};

#define lfs_checkpoint_magic   0xbf87ca91
struct lfs_b_checkpoint {
	uint32_t magic;
	uint32_t crc;

	uint32_t timestamp_a;

	uint32_t seg_head;
	uint32_t seg_tail;
#define lfs_seg_free   0xffffffff
	uint32_t segs[];

	/* uint32_t timestamp_b == timestamp_a */
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

	bool cached;
	int mem_fid;
	void *addr;

	struct lfs_blk *bnext;

	struct lfs_blk *wnext;
};

struct lfs_inode {
	uint32_t inode_idx;

	char name[16];
	uint32_t blk_cnt;
	uint64_t len_real;
	
	uint32_t inode_blk;

	struct lfs_blk *blks;
};

struct lfs_segment {
	uint32_t blk;
	uint32_t index;
	struct lfs_segment *prev;
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
	
	uint32_t timestamp;

	size_t n_segs;
	struct lfs_segment *segments;
	struct lfs_segment *seg_head;
	struct lfs_segment *seg_tail;
	struct lfs_segment *seg_free;

	size_t n_inodes;
	struct lfs_inode *inodes;

	struct lfs_blk *pending;
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
block_read(int cid, int fid, size_t start_blk, size_t len);

int
block_write(int cid, int fid, size_t start_blk, size_t len);

int
block_get_block_size(int cid, size_t *blk_size);

