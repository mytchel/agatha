struct lfs_blk_info {
	uint32_t blk;
	uint32_t crc;
};

struct lfs_inode {
	uint32_t magic;
	uint32_t crc;

	uint32_t inode_idx;
	uint8_t name[16];

	uint32_t nblks;

#define n_direct_blks 16
	struct lf2_blk_info direct[n_direct_blks];
	struct lf2_blk_info indirect;
	struct lf2_blk_info double_indirect;
};

#define lfs_seg_size  (1<<20)

struct lfs_seg_sum {
	uint32_t magic;
	uint32_t crc;

#define lfs_blk_free    0
#define lfs_blk_inode   1

	uint32_t blk_inode[255];
};

struct lf2_checkpoint {
	uint32_t magic;
	uint32_t crc;

	uint32_t timestamp_a;

#define lfs_seg_free   0xffffffff
	uint32_t segs[];

	/* uint32_t timestamp_b == timestamp_a */
};

struct lfs_superblock {
	uint32_t magic;
	uint32_t crc;

	uint32_t blk_size;
	uint32_t seg_size;
	uint32_t seg_count;
};


