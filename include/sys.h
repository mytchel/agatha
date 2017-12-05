#define MESSAGE_LEN 64

/* Permitions.
   Or with architecture depended map type. */
   
#define F_MAP_READ             (1<<0)
#define F_MAP_WRITE            (1<<1)
#define F_MAP_TYPE_SHIFT       2
#define F_MAP_TYPE_MASK        (0xf<<F_MAP_TYPE_SHIFT)

typedef struct frame *frame_t;

struct frame {
	int f_id;
	
	int type;
	size_t pa, len;
	
	/* f_id of table if mapped to one. */
	int t_id;
  size_t t_va;
	size_t va;
	
  /* Mapping flags. */	
	int flags;
};

