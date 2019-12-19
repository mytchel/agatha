union block_req {
	uint8_t raw[MESSAGE_LEN];
	uint32_t type;

	struct {
		uint32_t type;
	} info;

	struct {
		uint32_t type;
		size_t blk;
		size_t n_blks;
		size_t off;
	} read;

	struct {
		uint32_t type;
		size_t blk;
		size_t n_blks;
		size_t off;
	} write;
};

union block_rsp {
	uint8_t raw[MESSAGE_LEN];

	struct {
		uint32_t type;
		int ret;
	} untyped;

	struct {
		uint32_t type;
		int ret;
		size_t block_size;
		size_t nblocks;
	} info;

	struct {
		uint32_t type;
		int ret;
	} read;

	struct {
		uint32_t type;
		int ret;
	} write;
};

struct block_dev {
	void *arg;
	char *name;

	int mount_cap;
	
	bool writable;
	size_t block_size;
	size_t nblocks;
	
	bool map_buffers;

	int (*read_blocks_mapped)(struct block_dev *,
			void *buf,
			size_t blk, 
			size_t n);

	int (*write_blocks_mapped)(struct block_dev *,
			void *buf,
			size_t blk, 
			size_t n);

	int (*read_blocks)(struct block_dev *,
			size_t pa,
			size_t blk, 
			size_t n);

	int (*write_blocks)(struct block_dev *,
			size_t pa,
			size_t blk, 
			size_t n);
};

int
block_dev_register(struct block_dev *dev);


