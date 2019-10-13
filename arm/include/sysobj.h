typedef enum {
	OBJ_untyped = 0,
	OBJ_endpoint,
	OBJ_caplist,
	OBJ_proc,
	OBJ_intr,
	OBJ_type_n
} obj_type_t;

int
obj_retype(int cid, obj_type_t type, size_t n);

