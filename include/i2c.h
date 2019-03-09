union i2c_req {
	uint8_t raw[MESSAGE_LEN];
	uint32_t type;

	struct {
		uint32_t type;
		uint8_t addr;
		size_t speed_kHz;
	} configure;

	struct {
		uint32_t type;
		uint8_t slave, addr;
		size_t len;
	} read;

	struct {
		uint32_t type;
		uint8_t slave, addr;
		size_t len;
		uint8_t buf[54];
	} write;
};

union i2c_rsp {
	uint8_t raw[MESSAGE_LEN];

	struct {
		uint32_t type;
		int ret;
	} untyped;

	struct {
		uint32_t type;
		int ret;
	} info;

	struct {
		uint32_t type;
		int ret;
		uint8_t buf[54];
	} read;

	struct {
		uint32_t type;
		int ret;
	} write;
};

