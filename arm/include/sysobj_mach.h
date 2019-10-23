#define OBJ_intr    4
#define OBJ_type_n  5

typedef struct obj_intr obj_intr_t;

struct obj_intr {
	struct obj_head h;

	size_t irqn;
	obj_endpoint_t *end;
	uint32_t signal;
};

