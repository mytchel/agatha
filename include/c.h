#include <syscall.h>

#define align_up(x, a)  (((x) + a - 1) & (~(a-1)))

	void
raise(void)
  __attribute__((noreturn));

bool
cas(void *addr, void *old, void *new);

void
memcpy(void *dst, const void *src, size_t len);

void
memset(void *dst, uint8_t v, size_t len);

bool
memcmp(const void *a, const void *b, size_t len);

/* Address space management */

int
unmap_addr(void *a, size_t len);

void *
map_addr(size_t pa, size_t len, int flags);

size_t
request_memory(size_t len);

int
release_addr(size_t pa, size_t len);

int
give_addr(int to, 
	size_t pa, size_t len);

/* Lib utils */

void *
malloc(size_t size);

void
free(void *ptr);

int
kcap_alloc(void);

void
kcap_free(int cid);

int
kobj_alloc(int type, size_t n);

void
kobj_free(int cid);

