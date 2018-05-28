#include "mmu.h"

void
init_mem(void);

size_t
get_mem(size_t l, size_t align);

void
free_mem(size_t a, size_t l);

void *
map_free(size_t pa, size_t len, int ap, bool cache);

void
unmap(void *va, size_t len);

struct pool_frame {
	struct pool_frame *next;
	size_t pa, len;
	size_t use[];
};

struct pool {
	struct pool_frame *head;
	size_t obj_size;
	size_t nobj;
	size_t frame_size;

	struct pool *next;
};

struct pool *
pool_new(size_t obj_size);

void *
pool_alloc(struct pool *s);

void
pool_free(struct pool *s, void *p);

void
init_procs(void);

size_t
proc_map(size_t pid, size_t pa, size_t va, size_t len, int flags);

extern struct kernel_info *info;

