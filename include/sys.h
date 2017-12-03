#define MESSAGE_LEN 64

/* Permitions.
   Or with architecture depended map type. */
   
#define F_MAP_READ         (1<<0)
#define F_MAP_WRITE        (1<<1)
#define F_MAP_TYPE_SHIFT       2
#define F_MAP_TYPE_MASK      (0xf<<F_MAP_TYPE_SHIFT)

typedef struct frame *frame_t;

struct frame {
	int f_id;
	
	int type;
	size_t pa, len;
	
	/* f_id of vspace if mapped to one. */
	int v_id;
	/* Mapping as a page/section/something at. */
	size_t va;
	/* Mapped as a table at. */
	size_t t_va;
	/* Mapping flags. */	
	int flags;
};

