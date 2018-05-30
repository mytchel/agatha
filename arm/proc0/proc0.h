#include "mmu.h"

struct l3 {
	struct l3 *next;

	size_t pa, va, len;
	int flags;
};

struct l2 {
	struct l2 *next;

	size_t pa, va, len;
	struct l3 *head;
	void *addr;
};

struct l1 {
	size_t pa, len;
	void *addr;
	struct l2 *head;
};

struct proc {
	int pid;
	struct l1 *l1;
};

void
init_mem(void);

size_t
get_mem(size_t l, size_t align);

void
free_mem(size_t a, size_t l);

void *
map_free(size_t pa, size_t len, int flags);

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

	struct l1 *
l1_create_from(size_t pa, void *va, size_t len);

	struct l1 *
l1_create(void);

	void
l1_free(struct l1 *l);

struct l2 *
l2_create_table_from(size_t len, size_t va, size_t pa, void *addr);

struct l2 *
l2_create_table(size_t len, size_t va);

	void
l2_free(struct l2 *l);

	int
l1_insert_l2(struct l1 *l1, struct l2 *l2);

	struct l2 *
l1_get_l2(struct l1 *l1, size_t va);

	int
l2_insert_l3(struct l2 *l2, struct l3 *l3);

	size_t
l1_free_va(struct l1 *l1, size_t len);

struct l3 *
l3_create(size_t pa, size_t va, size_t len, int flags);

	void
l3_free(struct l3 *l);

void
init_procs(void);

size_t
proc_map(size_t pid, size_t pa, size_t va, size_t len, int flags);

extern struct kernel_info *info;
extern struct proc procs[];


