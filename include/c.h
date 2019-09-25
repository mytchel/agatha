/* Syscalls */

extern int parent_eid;

int
yield(void);

int
mesg(int eid, void *rq, void *rp);

int
recv(int eid, int *pid, void *m);

int
reply(int eid, int pid, void *recv);

int
signal(int eid, uint32_t s);

int
endpoint_create(void);

int
endpoint_offer(int eid);

int
endpoint_accept(int eid, int pid);

int
pid(void);

void
exit(uint32_t code)
	__attribute__((noreturn));

int
intr_exit(void);

/* Utils */

#define align(x, a)  (((x) + a - 1) & (~(a-1)))

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

#define MAP_RO         (0<<0)
#define MAP_RW         (1<<0)

#define MAP_TYPE_MASK  (7<<1) 

#define MAP_MEM        (0<<1)
#define MAP_DEV        (1<<1) 
#define MAP_SHARED     (2<<1) 

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


void *
malloc(size_t size);

void
free(void *ptr);

