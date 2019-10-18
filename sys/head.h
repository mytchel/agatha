#include <types.h>
#include <mach.h>
#include <sys.h>
#include <sysnum.h>
#include <sysobj.h>
#include <err.h>
#include <mesg.h>
#include <stdarg.h>
#include <string.h>

typedef struct proc proc_t;
typedef struct proc_list proc_list_t;

typedef struct cap cap_t;
typedef struct obj_head obj_head_t;
typedef struct obj_untyped obj_untyped_t;
typedef struct obj_endpoint obj_endpoint_t;
typedef struct obj_caplist obj_caplist_t;
typedef struct obj_proc obj_proc_t;
typedef struct obj_intr obj_intr_t;

struct proc_list {
	proc_t *head, *tail;
};

#define CAP_write  1
#define CAP_read   2

struct cap {
	cap_t *prev, *next;
	int id;
	uint32_t perm;
	obj_head_t *obj;
};

struct obj_head {
	size_t refs;
	int type;
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

struct obj_caplist {
	struct obj_head h;

	size_t n;
	cap_t caps[];
};

struct obj_proc {
	struct obj_head h;

	proc_t *proc;
};

struct obj_untyped {
	struct obj_head h;
	size_t len;
};

struct proc {
	label_t label;

	obj_proc_t *obj;

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
	cap_t *give, *take;

	proc_t *wprev, *wnext;

	obj_endpoint_t *recv_from;

	int next_cap_id;
	cap_t *caps;
	cap_t cap0;
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



#define align_up(x, a) (((x) + a - 1) & (~(a-1)))

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


cap_t *
cap_create(proc_t *p);

void
cap_free(proc_t *p, cap_t *c);

void
cap_add(proc_t *p, cap_t *c);

void
cap_remove(proc_t *p, cap_t *c);


size_t
obj_untyped_size(size_t n);

size_t
obj_endpoint_size(size_t n);

size_t
obj_caplist_size(size_t n);

size_t
obj_proc_size(size_t n);

extern size_t (*obj_size_funcs[OBJ_type_n])(size_t n);

int
obj_untyped_init(proc_t *p, void *o, size_t n);

int
obj_endpoint_init(proc_t *p, void *o, size_t n);

int
obj_caplist_init(proc_t *p, void *o, size_t n);

int
obj_proc_init(proc_t *p, void *o, size_t n);

extern int (*obj_init_funcs[OBJ_type_n])(proc_t *p, void *o, size_t n);

cap_t *
proc_find_cap(proc_t *p, int cid);


size_t
sys_yield(void);

size_t
sys_pid(void);

size_t
sys_exit(uint32_t code);

size_t
sys_obj_create(size_t pa, size_t len);

size_t
sys_obj_retype(int cid, int type, size_t n);

size_t
sys_obj_split(int cid);

size_t
sys_obj_merge(int cid_l, int cid_h);

size_t
sys_proc_setup(int cid, size_t vspace, size_t priority, int p_eid);

size_t
sys_proc_start(int cid, size_t pc, size_t sp);

size_t
sys_mesg(int to, uint8_t *rq, uint8_t *rp, int *cid);

size_t
sys_recv(int from, int *pid, uint8_t *m, int *cid);

size_t
sys_reply(int to, int pid, uint8_t *m, int cid);

size_t
sys_signal(int to, uint32_t s);

size_t
sys_endpoint_create(void);

size_t
sys_endpoint_connect(int cid);

size_t
sys_debug(char *s);


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
		size_t pc,
		size_t arg0,
		size_t arg1);

void
proc_start(size_t pc, size_t sp) __attribute__((noreturn));

bool
cas(void *addr, void *old, void *new);

int
mmu_switch(size_t va_pa);

void
set_systick(size_t ticks);

size_t
systick_passed(void);

void *
kernel_map(size_t pa, size_t len, bool cache);

void
kernel_unmap(void *addr, size_t len);

extern proc_t *up;

extern void (*debug_puts)(const char *);

