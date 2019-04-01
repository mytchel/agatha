/* Syscalls */

int
yield(void);

int
send(int to, void *m);

int
recv(int from, void *m);

int
mesg(int to, void *send, void *recv);

int
pid(void);

void
exit(void)
	__attribute__((noreturn));

int
intr_exit(void);

/* Utils */

	void
raise(void)
  __attribute__((noreturn));

bool
cas(void *addr, void *old, void *new);

void
memcpy(void *dst, const void *src, size_t len);

void
memset(void *dst, uint8_t v, size_t len);

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


