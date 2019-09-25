#define ROOT_PID          1

#define MESSAGE_LEN      64

#define PID_NONE          0
#define PID_SIGNAL        0
#define EID_ANY           0

#define PRIORITY_MAX  16

typedef enum {
	PROC_free = 0,
	PROC_fault,

	PROC_ready,

	PROC_block_recv,
	PROC_block_send,
	PROC_block_reply,
} procstate_t;

union proc_msg {
	uint8_t raw[MESSAGE_LEN];
	uint32_t type;

	struct {
		uint32_t type;
		size_t pc;
		size_t sp;
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

