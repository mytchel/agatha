struct block_dev {
	void *arg;
	size_t block_len;

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


