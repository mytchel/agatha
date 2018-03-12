#include "mmu.h"

struct frame {
	size_t pa;
	size_t len;

	int pid;
	size_t va;

	struct frame *next;
};

void
init_procs(void);

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

void
init_l1(uint32_t *t);

struct frame *
get_frame(void);

