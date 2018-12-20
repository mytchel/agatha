#define BLOCK_DEV_info   0
#define BLOCK_DEV_read   1
#define BLOCK_DEV_write  2

union block_dev_req {
	uint8_t raw[MESSAGE_LEN];
	int type;

	struct {
		int type;
	} info;

	struct {
		int type;
		size_t pa, len;
		size_t start;
		size_t r_len;
	} read;

	struct {
		int type;
		size_t pa, len;
		size_t start;
		size_t w_len;
	} write;
};

union block_dev_rsp {
	uint8_t raw[MESSAGE_LEN];

	struct {
		int type;
		int ret;
	} untyped;

	struct {
		int type;
		int ret;
		size_t block_size;
		size_t nblocks;
	} info;

	struct {
		int type;
		int ret;
	} read;

	struct {
		int type;
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


