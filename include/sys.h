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

