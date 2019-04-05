union block_req {
	uint8_t raw[MESSAGE_LEN];
	uint32_t type;

	struct {
		uint32_t type;
	} info;

	struct {
		uint32_t type;
		size_t pa, len;
		size_t start;
		size_t r_len;
	} read;

	struct {
		uint32_t type;
		size_t pa, len;
		size_t start;
		size_t w_len;
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
	size_t block_size;
	size_t nblocks;

	char *name;

	int (*read_blocks)(struct block_dev *,
			void *buf,
			size_t index, 
			size_t n);

	int (*write_blocks)(struct block_dev *,
			void *buf,
			size_t index, 
			size_t n);
};

int
block_dev_register(struct block_dev *dev);

