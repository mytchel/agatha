#define PROC0_PID      0

#define PROC0_irq      0

#define PROC0_irq_reg  2
#define PROC0_irq_req  3
#define PROC0_proc     4 
#define PROC0_addr_req 5
#define PROC0_addr_map 6

struct proc0_irq {
	int type;
	size_t irq;
};

union proc0_req {
	uint8_t raw[MESSAGE_LEN];
	int type;

	struct {
		int type;
		size_t pa;
		size_t len;
	} addr_req;

	struct {
		int type;
		size_t pa;
		size_t va;
		size_t len;
		int flags;
	} addr_map;

	struct {
		int type;
		size_t irqn;
	} irq_reg;

	struct {
		int type;
		size_t irqn;
	} irq_req;

	struct {
		int type;
		int flags;
	} proc;
};

union proc0_rsp {
	uint8_t raw[MESSAGE_LEN];

	struct {
		int type;
		int ret;
	} untyped;

	struct {
		int type;
		int ret;
		size_t pa;
	} addr_req;

	struct {
		int type;
		int ret;
	} addr_map;

	struct {
		int type;
		int ret;
	} irq_reg;

	struct {
		int type;
		int ret;
	} irq_req;

	struct {
		int type;
		int ret;
		int pid;
	} proc;
};

