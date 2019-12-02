#define OBJ_intr    4
#define OBJ_frame   5
#define OBJ_type_n  6

typedef struct obj_intr obj_intr_t;
typedef struct obj_frame obj_frame_t;

struct obj_intr {
	struct obj_head h;

	size_t irqn;
	obj_endpoint_t *end;
	uint32_t signal;
};

#define FRAME_NONE   0
#define FRAME_MEM    1
#define FRAME_DEV    2
#define FRAME_L1     3
#define FRAME_L2     4

struct obj_frame {
	struct obj_head h;

	size_t pa, len;
	int type;

	size_t va;
	obj_frame_t *next;
};

