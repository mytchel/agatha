int
yield(void);

int
send(int pid, uint8_t *m);

int
recv(uint8_t *m);

void
raise(void)
  __attribute__((noreturn));

bool
cas(void *addr, void *old, void *new);

void
memcpy(void *dst, const void *src, size_t len);

void
memset(void *dst, uint8_t v, size_t len);

/* Proc0 syscalls */

int
proc_new(void);

int
va_table(int p_id, size_t pa);

