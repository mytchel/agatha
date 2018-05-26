#include <types.h>
#include <mach.h>
#include <sys.h>
#include <err.h>
#include <stdarg.h>
#include <string.h>

#define MAX_PROCS       4
#define KSTACK_LEN   1024

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
	
  uint8_t kstack[KSTACK_LEN];

	uint8_t m[MESSAGE_LEN];
	uint8_t intr;
	proc_t waiting_on, wnext;
	proc_t waiting;
	
	size_t vspace;
};

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

int
send_intr(proc_t p, size_t intr);

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
jump(size_t)
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
mmu_switch(size_t va_pa);

void
set_systick(size_t ms);

int
add_kernel_irq(size_t irqn, void (*func)(size_t));

int
add_user_irq(size_t irqn, proc_t p);

void
fill_intr_m(uint8_t *, size_t irqn);

void
puts(const char *);

/* Variables. */

extern proc_t up;

