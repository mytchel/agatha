#define PROC0_PID      0

#define PROC0_mem_req  0
#define PROC0_dev_register 1
#define PROC0_dev_req  2
#define PROC0_irq_req  3
#define PROC0_proc     4
#define PROC0_irq      5

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
			size_t addr;
			size_t len;
			int flags;
		} mem_req;

		struct {
			char compat[32];
		} dev_register;

		struct {
			int id;
			int n;
		} dev_req;

		struct {
			size_t irqn;
		} irq_req;

		struct {
			int flags;
		} proc;
	} m;
};

struct proc0_rsp {
	size_t from;
	uint8_t type;
	int ret;
	union {
		struct {
			size_t addr;
		} mem_req;

		struct {
			int id;
		} dev_register;

		struct {
			size_t addr;
			size_t len;
			int irq;
		} dev_req;

		struct {
		} irq_req;

		struct {
			int pid;
		} proc;
	} m;
};

