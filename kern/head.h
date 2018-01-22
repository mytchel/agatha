#include <types.h>
#include <mach.h>
#include <sys.h>
#include <err.h>
#include <stdarg.h>
#include <string.h>

#define MAX_PROCS      16
#define MAX_FRAMES    512
#define KSTACK_LEN   1024

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
	
	size_t frame_count;
	
	kframe_t frames;
	kframe_t vspace;
	
  uint8_t kstack[KSTACK_LEN];
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
frame_find_fid(proc_t p, int f_id);

kframe_t
frame_find_ind(proc_t p, int ind);

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

void
proc_start(void)
  __attribute__((noreturn));

bool
cas(void *addr, void *old, void *new);

int
frame_map(kframe_t t, kframe_t f, size_t va, int flags);

int
frame_table(kframe_t f, int flags);

/* If f is a table gives frames mapped to the table also. */
int
frame_give(proc_t from, proc_t to, kframe_t f);

int
mmu_switch(kframe_t f);

void
set_systick(size_t ms);

int
register_irq(proc_t p, size_t irq);

/* Variables. */

extern proc_t up;

extern struct kernel_info kernel_info;

