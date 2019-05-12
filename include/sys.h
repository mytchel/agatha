#define MESSAGE_LEN 64
#define PID_ANY    -1
#define PID_NONE   -2

typedef enum {
	PROC_free = 0,
	PROC_fault,

	PROC_ready,
	PROC_recv,
} procstate_t;

union proc_msg {
	uint8_t raw[MESSAGE_LEN];
	uint32_t type;

	struct {
		uint32_t type;
		size_t pc;
		size_t sp;
		size_t arg;
	} start;

	struct {
		uint32_t type;
		size_t pc;
		int fault_flags;
		size_t data_addr;
	} fault;

	struct {
		size_t type;
		uint32_t code;
		char message[MESSAGE_LEN - sizeof(uint8_t) * 2];
	} exit;
};

