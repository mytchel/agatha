union serial_req {
	uint8_t raw[MESSAGE_LEN];
	uint32_t type;

	struct {
		uint32_t type;
		size_t len;
	 	char data[MESSAGE_LEN - sizeof(uint32_t) - sizeof(size_t)];	
	} write __attribute__((packed));

	struct {
		uint32_t type;
		size_t len;
		bool break_on_newline;
	} read;
};

union serial_rsp {
	uint8_t raw[MESSAGE_LEN];
	uint32_t type;

	struct {
		uint32_t type;
		int ret;
	} write;

	struct {
		uint32_t type;
		int ret;
		size_t len;
	 	char data[MESSAGE_LEN - sizeof(uint32_t) - sizeof(size_t) - sizeof(int)];	
	} read __attribute__((packed));
};

