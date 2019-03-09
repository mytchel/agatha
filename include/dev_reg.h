#define DEV_REG_PID       1

#define DEV_REG_name_len 32

union dev_reg_req {
	uint8_t raw[MESSAGE_LEN];
	uint32_t type;

	struct {
		uint32_t type;
		char name[DEV_REG_name_len];
		bool block;
	} find;

	struct {
		uint32_t type;
		int pid;
		char name[DEV_REG_name_len];
	} reg;

	struct {
		uint32_t type;
		int id;
	} list;
};

union dev_reg_rsp {
	uint8_t raw[MESSAGE_LEN];

	struct {
		uint32_t type;
		int ret;
	} untyped;

	struct {
		uint32_t type;
		int ret;
		int pid;
		int id;
	} find;

	struct {
		uint32_t type;
		int ret;
		int id;
	} reg;

	struct {
		uint32_t type;
		int ret;
		int pid;
		char name[DEV_REG_name_len];
	} list;
};

int
get_device_pid(char *name);

