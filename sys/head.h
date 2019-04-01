#include <types.h>
#include <mach.h>
#include <sys.h>
#include <err.h>
#include <stdarg.h>
#include <string.h>

typedef struct message *message_t;

struct message {
	message_t next;
	int from;
	uint8_t body[MESSAGE_LEN];
};

typedef struct proc *proc_t;
typedef struct proc_list *proc_list_t;

typedef enum {
	PROC_dead,

	PROC_ready,
	PROC_recv,
} procstate_t;

struct proc_list {
	proc_t head, tail;
};

struct proc {
	label_t label;

	bool in_irq;
	label_t *irq_label;
	
	procstate_t state;
	int pid;

	int ts;
	proc_list_t list;		
	proc_t prev, next;
	
  uint8_t kstack[KSTACK_LEN];

	int recv_from;
	message_t messages;
	
	size_t vspace;
};

proc_t
proc_new(void);

void
proc_ready(proc_t p);

proc_t
find_proc(int pid);

void
schedule(proc_t next);

message_t
message_get(void);

void
message_free(message_t);

int
send(proc_t to, message_t);

int
recv(int from, uint8_t *m);

void
memcpy(void *dst, const void *src, size_t len);

void
memset(void *dst, uint8_t v, size_t len);

#define DEBUG_ERR     0
#define DEBUG_WARN    1
#define DEBUG_INFO    2
#define DEBUG_SCHED   3
#define DEBUG_SCHED_V 4

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DEBUG_ERR
#endif

void
debug(int code, const char *fmt, ...);

#if DEBUG_LEVEL >= DEBUG_ERR
#define debug_err(X, ...) debug(DEBUG_ERR, X, ##__VA_ARGS__) 
#else
#define debug_err(X, ...)
#endif

#if DEBUG_LEVEL >= DEBUG_WARN
#define debug_warn(X, ...) debug(DEBUG_WARN, X, ##__VA_ARGS__);
#else
#define debug_warn(X, ...)
#endif

#if DEBUG_LEVEL >= DEBUG_INFO
#define debug_info(X, ...) debug(DEBUG_INFO, X, ##__VA_ARGS__);

void
debug_dump_label(label_t *l);

#else
#define debug_info(X, ...)
#define debug_dump_label(X)

#endif

#if DEBUG_LEVEL >= DEBUG_SCHED
#define debug_sched(X, ...) debug(DEBUG_SCHED, X, ##__VA_ARGS__);
#else
#define debug_sched(X, ...)
#endif

#if DEBUG_LEVEL >= DEBUG_SCHED_V
#define debug_sched_v(X, ...) debug(DEBUG_SCHED, X, ##__VA_ARGS__);
#else
#define debug_sched_v(X, ...)
#endif


void
panic(const char *fmt, ...);

/* Machine dependant. */

int
set_label(label_t *l);

int
goto_label(label_t *l) __attribute__((noreturn));

void
drop_to_user(label_t *l)
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
mmu_switch(size_t va_pa);

void
set_systick(size_t ticks);

size_t
systick_passed(void);

int
irq_add_kernel(void (*func)(size_t), size_t irqn);

void
irq_clear_kernel(size_t irqn);

int
irq_add_user(struct intr_mapping *map);

int
irq_remove_user(size_t irqn);

int
irq_enter(size_t irqn);

int
irq_exit(void);

void
irq_run_active(void);

void
irq_handler(void);

extern proc_t up;

extern void (*debug_puts)(const char *);

