#define DEV_REG_PID       1

#define DEV_REG_find      0
#define DEV_REG_register  1
#define DEV_REG_list      2

#define DEV_REG_name_len 32

union dev_reg_req {
	uint8_t raw[MESSAGE_LEN];
	int type;

	struct {
		int type;
		char name[DEV_REG_name_len];
	} find;

	struct {
		int type;
		int pid;
		char name[DEV_REG_name_len];
	} reg;

	struct {
		int type;
		int id;
	} list;
};

union dev_reg_rsp {
	uint8_t raw[MESSAGE_LEN];

	struct {
		int type;
		int ret;
	} untyped;

	struct {
		int type;
		int ret;
		int pid;
		int id;
	} find;

	struct {
		int type;
		int ret;
		int id;
	} reg;

	struct {
		int type;
		int ret;
		int pid;
		char name[DEV_REG_name_len];
	} list;
};

