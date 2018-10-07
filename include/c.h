/* Syscalls */

int
yield(void);

int
send(int to, void *m);

int
recv(int from, void *m);

int
pid(void);

void
exit(void)
	__attribute__((noreturn));

/* Proc0 only syscalls */

int
proc_new(void);

int
va_table(int p_id, size_t pa);

int
intr_register(int p_id, size_t irq);

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

void *
request_device(size_t pa, size_t len, int flags);

int
release_addr(size_t pa, size_t len);

int
give_addr(int to, 
		size_t pa, size_t len);

