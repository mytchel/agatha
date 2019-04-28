#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <arm/mmu.h>
#include "kern.h"

struct addr_frame {
	struct addr_frame *next;
	size_t pa, len;
	int table;
	int mapped;
};

struct proc {
	struct addr_frame *frames;

	struct {
		size_t pa, len;
		uint32_t *table;
		uint32_t *mapped;
	} l1;
};

void
init_mem(void);

size_t
get_ram(size_t len, size_t align);

void
free_addr(size_t pa, size_t len);

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

extern struct pool *frame_pool;

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

struct addr_frame *
frame_new(size_t pa, size_t len);

void
frame_free(struct addr_frame *f);

int
proc_give_addr(int pid, struct addr_frame *f);

struct addr_frame *
proc_get_addr(int pid, size_t pa, size_t len);

struct addr_frame *
proc_take_addr(int pid, size_t pa, size_t len);

int
proc_map(int pid, size_t pa, size_t va, size_t len, int flags);

int
proc_unmap(int pid, size_t va, size_t len);




int
init_bundled_proc(char *name, size_t prog_pa, size_t len);

void
add_ram(size_t start, size_t len);

void
board_init_ram(void);

void
board_init_bundled_drivers(size_t offset_into_bundle);


extern struct kernel_info *info;


