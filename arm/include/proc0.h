#define PROC0_PID         0

#define MAP_RO         (0<<0)
#define MAP_RW         (1<<0)

#define MAP_TYPE_MASK  (7<<1) 

#define MAP_MEM        (0<<1)
#define MAP_DEV        (1<<1) 
#define MAP_SHARED     (2<<1) 
#define MAP_TABLE      (3<<1) 

#define MAP_REMOVE_LEAF  (4<<1) 
#define MAP_REMOVE_TABLE (5<<1) 

enum {
	PROC0_OK = OK,
	PROC0_ERR_INTERNAL = -2,
 	PROC0_ERR_FLAG_INCOMPATABLE = -3,
 	PROC0_ERR_FLAGS = -4,
 	PROC0_ERR_ALIGNMENT = -5,
 	PROC0_ERR_TABLE = -6,
 	PROC0_ERR_PERMISSION_DENIED = -7,
 	PROC0_ERR_ADDR_DENIED = -8,
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
		int to;
		size_t pa;
		size_t len;
	} addr_give;

	struct {
		uint32_t type;
		size_t irqn;
		void *func;
		void *arg;
		void *sp;
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

