#define PROC0_PID         0

enum {
	PROC0_ERR_INTERNAL,
 	PROC0_ERR_FLAG_INCOMPATABLE,
 	PROC0_ERR_FLAGS,
 	PROC0_ERR_ALIGNMENT,
 	PROC0_ERR_TABLE,
 	PROC0_ERR_PERMISSION_DENIED,
 	PROC0_ERR_ADDR_DENIED,
};

struct proc0_irq {
	uint32_t type;
	size_t irq;
};

union proc0_req {
	uint8_t raw[MESSAGE_LEN];
	uint32_t type;

	struct {
		uint32_t type;
		size_t pa;
		size_t len;
	} addr_req;

	struct {
		uint32_t type;
		size_t pa;
		size_t va;
		size_t len;
		int flags;
	} addr_map;

	struct {
		uint32_t type;
		size_t va;
		size_t len;
	} addr_unmap;

	struct {
		uint32_t type;
		int to;
		size_t pa;
		size_t len;
	} addr_give;

	struct {
		uint32_t type;
		size_t irqn;
	} irq_reg;

	struct {
		uint32_t type;
		int flags;
	} proc;
};

union proc0_rsp {
	uint8_t raw[MESSAGE_LEN];

	struct {
		uint32_t type;
		int ret;
	} untyped;

	struct {
		uint32_t type;
		int ret;
		size_t pa;
	} addr_req;

	struct {
		uint32_t type;
		int ret;
	} addr_map;

	struct {
		uint32_t type;
		int ret;
	} addr_unmap;

	struct {
		uint32_t type;
		int ret;
	} addr_give;

	struct {
		uint32_t type;
		int ret;
	} irq_reg;

	struct {
		uint32_t type;
		int ret;
		int pid;
	} proc;
};

