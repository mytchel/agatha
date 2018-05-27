#include "mmu.h"

struct slab_frame {
	struct slab_frame *next;
	size_t pa, len;
	size_t use[];
};

struct slab {
	struct slab_frame *head;
	size_t obj_size;
	size_t nobj;
	size_t frame_size;

	struct slab *next;
};

void
init_mem(void);

void
init_procs(void);

struct slab *
slab_new(size_t obj_size);

void *
slab_alloc(struct slab *s);

void
slab_free(struct slab *s, void *p);

size_t
get_mem(size_t l, size_t align);

void
free_mem(size_t a, size_t l);

void *
map_free(size_t pa, size_t len, int ap, bool cache);

void
unmap(void *va, size_t len);

void
init_l1(uint32_t *t);

size_t
proc_map(size_t pid, size_t pa, size_t va, size_t len, int flags);

extern struct kernel_info *info;

