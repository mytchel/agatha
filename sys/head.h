#include <types.h>
#include <mach.h>
#include <sys.h>
#include <err.h>
#include <mesg.h>
#include <stdarg.h>
#include <string.h>

typedef struct proc proc_t;
typedef struct proc_list proc_list_t;

typedef struct cap cap_t;
typedef union obj_untyped obj_untyped_t;
typedef struct obj_endpoint obj_endpoint_t;
typedef struct obj_intr obj_intr_t;

struct proc_list {
	proc_t *head, *tail;
};

typedef enum {
	OBJ_untyped = 0,
	OBJ_endpoint,
	OBJ_intr,
} obj_type_t;

struct obj_head {
	size_t refs;
	obj_type_t type;
};

struct obj_endpoint {
	struct obj_head h;

	proc_t *holder;
	uint32_t signal;
	proc_list_t waiting;
};

struct obj_intr {
	struct obj_head h;

	size_t irqn;
	obj_endpoint_t *end;
	uint32_t signal;
};

union obj_untyped {
	struct obj_head h;
	obj_endpoint_t endpoint;
	obj_intr_t intr;
};

#define CAP_write  1
#define CAP_read   2

struct cap {
	cap_t *next;
	int id;
	uint32_t perm;
	obj_untyped_t *obj;
};

struct cap_transfer {
	uint32_t perm;
	obj_untyped_t *obj;
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
	int m_ret;

	struct cap_transfer give;
	struct cap_transfer take;

	proc_t *wprev, *wnext;

	obj_endpoint_t *recv_from;

	int next_cap_id;
	cap_t *caps;
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



cap_t *
recv(cap_t *from, int *pid, uint8_t *m, int *cid);

int
reply(obj_endpoint_t *l, int pid, uint8_t *m, int cid);

int
mesg(obj_endpoint_t *c, uint8_t *rq, uint8_t *rp, int *cid);

int
signal(obj_endpoint_t *c, uint32_t s);



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

bool
intr_cap_claim(size_t irqn);

void
intr_cap_connect(size_t irqn, obj_endpoint_t *e, uint32_t s);

int
irq_add_kernel(size_t irqn, void (*func)(size_t));

void
irq_enable(size_t irqn);

void
irq_ack(size_t irqn);

extern proc_t *up;

extern void (*debug_puts)(const char *);

