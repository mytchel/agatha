int
proc_new(int f_id);

int
frame_map(int f_id, void *va, int flags);

int
vspace_create(void *table, int f_id);

int
vspace_swap(int pid, int f_id);

int
send(int pid, uint8_t *m);

int
recv(uint8_t *m);

bool
cas(void *addr, void *old, void *new);

void
memcpy(void *dst, const void *src, size_t len);

void
memset(void *dst, uint8_t v, size_t len);

