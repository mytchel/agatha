#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <stdarg.h>
#include <string.h>
#include <m.h>
#include <arm/mmu.h>

struct addr_frame {
	struct addr_frame *next;
	size_t pa, len;
	int table;
	int mapped;
};

struct proc {
	int pid;

	struct addr_frame *frames;

	struct {
		size_t pa, len;
		uint32_t *addr;	
	} l1;
};

void
init_mem(void);

int
get_regs(size_t pa, size_t len);

size_t
get_ram(size_t len, size_t align);

size_t
get_pa(size_t pa, size_t len);

void
release_addr(size_t pa, size_t len);

void *
map_free(size_t pa, size_t len, int ap, bool cache);

void
unmap(void *va, size_t len);

struct pool_frame {
	struct pool_frame *next;
	size_t pa, len, nobj;
	size_t use[];
};

struct pool {
	struct pool_frame *head;
	size_t obj_size;

	struct pool *next;
};

void
init_pools(void);

struct pool *
pool_new_with_frame(size_t obj_size, 
		void *frame, size_t len);

struct pool *
pool_new(size_t obj_size);

void
pool_destroy(struct pool *s);

void *
pool_alloc(struct pool *s);

void
pool_free(struct pool *s, void *p);

void
init_procs(void);

int
proc_give(size_t pid, size_t pa, size_t len);

int
proc_map(size_t pid, size_t pa, size_t va, size_t len, int flags);

extern struct kernel_info *info;
extern struct proc procs[];


