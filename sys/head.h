#include <types.h>
#include <mach.h>
#include <sys.h>
#include <err.h>
#include <mesg.h>
#include <stdarg.h>
#include <string.h>

typedef struct proc proc_t;
typedef struct proc_list proc_list_t;
typedef struct capability capability_t;

typedef struct endpoint_listen endpoint_listen_t;
typedef struct endpoint_connect endpoint_connect_t;
typedef struct interrupt interrupt_t;

struct proc_list {
	proc_t *head, *tail;
};

struct proc {
	label_t label;

	procstate_t state;
	int pid;
	
	uint32_t kstack[KSTACK_LEN/sizeof(uint32_t)];

	int priority;

	int ts;
	proc_list_t *list;		
	proc_t *sprev, *snext;

	size_t vspace;

	uint8_t m[MESSAGE_LEN];
	proc_t *wprev, *wnext;

	endpoint_listen_t *recv_from;

	capability_t *caps;
	capability_t *offering;
	capability_t *offered;
};

typedef enum {
	CAP_endpoint_listen,
	CAP_endpoint_connect,
	CAP_interrupt,
} capability_type_t;

struct endpoint_listen {
	proc_t *holder;
	uint32_t signal;
	proc_list_t waiting;
};

struct endpoint_connect {
	endpoint_listen_t *other;
};

struct interrupt {
	size_t irqn;
	endpoint_listen_t *other;
	uint32_t signal;
};

struct capability {
	capability_t *next;
	int id;

	capability_type_t type;

	union {
		endpoint_listen_t listen;
		endpoint_connect_t connect;
		interrupt_t interrupt;
	} c;
};

proc_t *
proc_new(int priority, size_t vspace);

int
proc_ready(proc_t *p);

int
proc_fault(proc_t *p);

int
proc_free(proc_t *p);

proc_t *
find_proc(int pid);

void
schedule(proc_t *next);


void
capability_offer(capability_t *c);

capability_t *
capability_accept(void);



capability_t *
recv(capability_t *from, int *pid, uint8_t *m);

int
reply(endpoint_listen_t *l, int pid, uint8_t *m);

int
mesg(endpoint_listen_t *c, uint8_t *rq, uint8_t *rp);

int
signal(endpoint_listen_t *c, uint32_t s);



void
memcpy(void *dst, const void *src, size_t len);

void
memset(void *dst, uint8_t v, size_t len);

#define DEBUG_ERR       1
#define DEBUG_WARN      2
#define DEBUG_INFO      4
#define DEBUG_SCHED     8
#define DEBUG_SCHED_V  16
#define DEBUG_INFO_V   32

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DEBUG_ERR
#endif

void
debug(int code, const char *fmt, ...);

#if DEBUG_LEVEL & DEBUG_ERR
#define debug_err(X, ...) debug(DEBUG_ERR, X, ##__VA_ARGS__) 
#else
#define debug_err(X, ...)
#endif

#if DEBUG_LEVEL & DEBUG_WARN
#define debug_warn(X, ...) debug(DEBUG_WARN, X, ##__VA_ARGS__);
#else
#define debug_warn(X, ...)
#endif

#if DEBUG_LEVEL & DEBUG_INFO
#define debug_info(X, ...) debug(DEBUG_INFO, X, ##__VA_ARGS__);
#else
#define debug_info(X, ...)
#endif

#if DEBUG_LEVEL & DEBUG_SCHED
#define debug_sched(X, ...) debug(DEBUG_SCHED, X, ##__VA_ARGS__);
#else
#define debug_sched(X, ...)
#endif

#if DEBUG_LEVEL & DEBUG_SCHED_V
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
drop_to_user(label_t *l) __attribute__((noreturn));

void
raise(uint32_t r0) __attribute__((noreturn));

void
func_label(label_t *l, 
		size_t stack, 
		size_t stacklen,
		size_t pc);

void
proc_start(void) __attribute__((noreturn));

bool
cas(void *addr, void *old, void *new);

int
mmu_switch(size_t va_pa);

void
set_systick(size_t ticks);

size_t
systick_passed(void);

capability_t *
get_user_int_cap(size_t irqn);

int
irq_add_kernel(size_t irqn, void (*func)(size_t));

void
irq_enable(size_t irqn);

void
irq_ack(size_t irqn);

extern proc_t *up;

extern void (*debug_puts)(const char *);

