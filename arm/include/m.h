#define PROC0_PID      0

#define PROC0_irq      0

#define PROC0_irq_reg  2
#define PROC0_irq_req  3
#define PROC0_proc     4 
#define PROC0_addr_req 5
#define PROC0_addr_map 6

struct proc0_irq {
	size_t from;
	int type;
	size_t irq;
};

struct proc0_req {
	size_t from;
	int type;
	union {
		struct {
			size_t pa;
			size_t len;
		} addr_req;

		struct {
			size_t pa;
			size_t va;
			size_t len;
			int flags;
		} addr_map;

		struct {
			size_t irqn;
		} irq_reg;

		struct {
			size_t irqn;
		} irq_req;

		struct {
			int flags;
		} proc;

		uint8_t raw[MESSAGE_LEN - sizeof(size_t) - sizeof(int)];
	} m;
};

struct proc0_rsp {
	size_t from;
	uint8_t type;
	int ret;
	union {
		struct {
			size_t pa;
		} addr_req;

		struct {
			int pid;
		} proc;
		
		uint8_t raw[MESSAGE_LEN - sizeof(size_t) - sizeof(int) - sizeof(int)];
	} m;
};

