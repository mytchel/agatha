int
proc_new(int f_id);

int
frame_create(size_t start, size_t len, int type);

int
frame_table(int f_id, int type);

int
frame_map(int t_id, int f_id, void *va, int flags);

int
frame_count(void);

int
frame_info_index(struct frame *f, int i);

int
frame_info(struct frame *f, int f_id);

int
frame_split(int f_id, size_t offset);

int
frame_merge(int a_if, int b_id);

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

