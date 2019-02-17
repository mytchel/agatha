#define I2C_configure   0
#define I2C_read   1
#define I2C_write  2

union i2c_req {
	uint8_t raw[MESSAGE_LEN];
	int type;

	struct {
		int type;
		uint8_t addr;
		size_t speed_kHz;
	} configure;

	struct {
		int type;
		uint8_t slave, addr;
		size_t len;
	} read;

	struct {
		int type;
		uint8_t slave, addr;
		size_t len;
		uint8_t buf[54];
	} write;
};

union i2c_rsp {
	uint8_t raw[MESSAGE_LEN];

	struct {
		int type;
		int ret;
	} untyped;

	struct {
		int type;
		int ret;
	} info;

	struct {
		int type;
		int ret;
		uint8_t buf[54];
	} read;

	struct {
		int type;
		int ret;
	} write;
};

