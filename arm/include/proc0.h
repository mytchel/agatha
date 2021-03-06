#define MAP_TABLE        (3<<1) 

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

union proc0_req {
	uint8_t raw[MESSAGE_LEN];
	uint32_t type;

	struct {
		uint32_t type;
		size_t len;
		size_t align;
	} mem_req;

	struct {
		uint32_t type;

#define RESOURCE_type_none       0
#define RESOURCE_type_log        1
#define RESOURCE_type_regs       2
#define RESOURCE_type_int        3
#define RESOURCE_type_timer      4
#define RESOURCE_type_serial     5
#define RESOURCE_type_block      6
#define RESOURCE_type_net        7
#define RESOURCE_type_mount      8

		int resource_type;
	} get_resource;
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
	} mem_req;

	struct {
		uint32_t type;
		int ret;
		union {
			struct {
				size_t pa, len;
			} regs;
			int irqn;
		} result;
	} get_resource;
};

