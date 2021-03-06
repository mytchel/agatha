#define OBJ_untyped  0
#define OBJ_endpoint 1
#define OBJ_caplist  2
#define OBJ_proc     3

typedef enum {
	PROC_fault = 0,
	PROC_ready,
	PROC_block_recv,
	PROC_block_send,
	PROC_block_reply,
} procstate_t;

typedef struct proc_list proc_list_t;

typedef struct cap cap_t;
typedef struct obj_head obj_head_t;
typedef struct obj_untyped obj_untyped_t;
typedef struct obj_endpoint obj_endpoint_t;
typedef struct obj_caplist obj_caplist_t;
typedef struct obj_proc obj_proc_t;

struct proc_list {
	obj_proc_t *head, *tail;
};

#define CAP_write  1
#define CAP_read   2

struct cap {
	uint32_t perm;
	obj_head_t *obj;
};

/* Need to add size */
struct obj_head {
	size_t refs;
	int type;
};

struct obj_untyped {
	struct obj_head h;
	size_t len;
};

struct obj_endpoint {
	struct obj_head h;

	uint32_t signal;

	obj_proc_t *bound;
	obj_proc_t *rwait;

	proc_list_t waiting;
};

#define CLIST_CAPS 510
struct obj_caplist {
	struct obj_head h;
	cap_t caps[CLIST_CAPS];
};

struct obj_proc {
	struct obj_head h;

	label_t label;

	procstate_t state;
	int pid;
	
	uint32_t kstack[KSTACK_LEN/sizeof(uint32_t)];

	int priority;

	int ts;
	proc_list_t *slist;
	obj_proc_t *sprev, *snext;

	size_t vspace;

	uint32_t bound_signal;
	uint8_t m[MESSAGE_LEN];
	int m_ret;
	cap_t *give, *take;

	obj_proc_t *wprev, *wnext;

	obj_caplist_t *cap_root;
};

#include <sysobj_mach.h>

