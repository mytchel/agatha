struct pool_obj {
	struct pool_obj *next;
	bool in_use;
	uint8_t body[]__attribute__((__aligned__(4)));
};

struct pool_frame {
	struct pool_frame *next;
	size_t len, nobj;
	uint8_t body[]__attribute__((__aligned__(4)));
};

struct pool {
	struct pool_frame *frames;
	struct pool_obj *objs;
	size_t n_free;

	size_t obj_size;
};

int
pool_init(struct pool *p, size_t obj_size);

int
pool_destroy(struct pool *p);

int
pool_load(struct pool *p, void *f, size_t len);

size_t
pool_n_free(struct pool *p);

void *
pool_alloc(struct pool *p);

void
pool_free(struct pool *s, void *p);

