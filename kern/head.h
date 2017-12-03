#include <sys.h>
#include <err.h>
#include <types.h>
#include <stdarg.h>
#include <string.h>

typedef struct kframe *kframe_t;

struct kframe {
	struct frame u;
	struct kframe *next;
};

typedef struct proc *proc_t;

typedef enum {
	PROC_dead,
	PROC_enter,
	PROC_ready,
	PROC_oncpu,
	PROC_send,
	PROC_recv,
} procstate_t;

struct proc {
	label_t label;
	
	procstate_t state;
	int pid;
	proc_t next;
	
	int m_from;
	uint8_t m[MESSAGE_LEN];
	
	uint8_t kstack[KSTACK_LEN];
	
	size_t frame_count;
	int frame_next_id;
	
	kframe_t frames;
	kframe_t vspace;
};

/* vspace should be the barest possible. */

proc_t
proc_new(void);

proc_t
find_proc(int pid);

void
schedule(proc_t next);

int
send(proc_t p, uint8_t *m);

int
recv(uint8_t *m);

kframe_t
frame_new(size_t pa, size_t len, int type);

void
frame_add(proc_t p, kframe_t f);

void
frame_remove(proc_t p, kframe_t f);

kframe_t
frame_split(kframe_t f, size_t offset);

int
frame_merge(kframe_t a, kframe_t b);

void
memcpy(void *dst, const void *src, size_t len);

void
memset(void *dst, uint8_t v, size_t len);

/* Machine dependant. */

int
debug(const char *fmt, ...);

void
panic(const char *fmt, ...);

void
set_systick(uint32_t ms);

intr_t
set_intr(intr_t i);

int
set_label(label_t *l);

int
goto_label(label_t *l) __attribute__((noreturn));

void
drop_to_user(label_t *l, 
             void *kstack, 
             size_t stacklen)
__attribute__((noreturn));

void
raise(void)
__attribute__((noreturn));

void
func_label(label_t *l, 
           size_t stack, 
           size_t stacklen,
           size_t pc);

bool
cas(void *addr, void *old, void *new);

int
frame_map(kframe_t v, void *tb, kframe_t f, size_t va, int flags);

int
vspace_create(void *tb, kframe_t f);

int
vspace_swap(proc_t from, proc_t to, kframe_t f);

int
mmu_switch(kframe_t f);

/* Variables. */

extern proc_t up;

