#include "head.h"

	size_t
sys_intr_init(int cid, int irqn)
{
	obj_intr_t *o;
	cap_t *i;

	debug_info("%i create intr %i\n", up->pid, irqn);

	if (up->pid != ROOT_PID) {
		return ERR;
	}

	i = proc_find_cap(up, cid);
	if (i == nil) {
		return ERR;
	}

	o = (void *) i->obj;
	if (o->h.type != OBJ_intr) {
		return ERR;
	}

	if (!intr_cap_claim(irqn)) {
		debug_warn("intr cap claim failed, TODO free resources\n");
		return ERR;
	}
	
	o->irqn = irqn;

	return OK;
}

	size_t
sys_intr_connect(int iid, int eid, uint32_t signal)
{
	obj_endpoint_t *oe;
	obj_intr_t *oi;
	cap_t *i, *e;

	debug_info("%i intr connect %i to %i\n", up->pid, iid, eid);

	i = proc_find_cap(up, iid);
	e = proc_find_cap(up, eid);

	if (i == nil || e == nil) {
		return ERR;
	} else if (i->obj->type != OBJ_intr) {
		return ERR;
	} else if (e->obj->type != OBJ_endpoint || !(e->perm & CAP_read)) {
		return ERR;
	}

	oi = (obj_intr_t *) i->obj;
	oe = (obj_endpoint_t *) e->obj;

	if (oi->irqn == 0) {
		debug_warn("%i tried to connect intr that hasn't been setup\n", 
			up->pid);
		return ERR;
	}

	oi->end = oe;
	oi->signal = signal;

	intr_cap_connect(oi->irqn,
		oi->end, oi->signal);
	
	return OK;
}

	size_t
sys_intr_ack(int iid)
{
	obj_intr_t *i;
	cap_t *c;

	debug_info("%i intr ack %i\n", up->pid, iid);

	c = proc_find_cap(up, iid);
	if (c == nil) {
		return ERR;
	} else if (c->obj->type != OBJ_intr) {
		return ERR;
	}

	i = (obj_intr_t *) c->obj;

	irq_ack(i->irqn);

	return OK;
}

size_t
obj_intr_size(size_t n)
{
	return sizeof(obj_intr_t);
}

int
obj_intr_init(obj_proc_t *p, void *o, size_t n)
{
	obj_intr_t *i = o;

	i->irqn = 0;
	i->end = nil;
	i->signal = 0;

	return OK;
}

size_t (*obj_size_funcs[OBJ_type_n])(size_t n) = {
	[OBJ_untyped]             = obj_untyped_size,
	[OBJ_endpoint]            = obj_endpoint_size,
	[OBJ_caplist]             = obj_caplist_size,
	[OBJ_proc]                = obj_proc_size,
	
	[OBJ_intr]                = obj_intr_size,
};

int (*obj_init_funcs[OBJ_type_n])(obj_proc_t *p, void *o, size_t n) = {
	[OBJ_untyped]             = obj_untyped_init,
	[OBJ_endpoint]            = obj_endpoint_init,
	[OBJ_caplist]             = obj_caplist_init,
	[OBJ_proc]                = obj_proc_init,
	
	[OBJ_intr]                = obj_intr_init,
};

void *systab[NSYSCALLS] = {
	[SYSCALL_YIELD]            = (void *) &sys_yield,
	[SYSCALL_PID]              = (void *) &sys_pid,
	[SYSCALL_EXIT]             = (void *) &sys_exit,

	[SYSCALL_OBJ_CREATE]       = (void *) &sys_obj_create,
	[SYSCALL_OBJ_RETYPE]       = (void *) &sys_obj_retype,
	[SYSCALL_OBJ_SPLIT]        = (void *) &sys_obj_split,
	[SYSCALL_OBJ_MERGE]        = (void *) &sys_obj_merge,

	[SYSCALL_MESG]             = (void *) &sys_mesg,
	[SYSCALL_RECV]             = (void *) &sys_recv,
	[SYSCALL_REPLY]            = (void *) &sys_reply,
	[SYSCALL_SIGNAL]           = (void *) &sys_signal,
	[SYSCALL_ENDPOINT_CONNECT] = (void *) &sys_endpoint_connect,

	[SYSCALL_PROC_SETUP]       = (void *) &sys_proc_setup,
	[SYSCALL_PROC_START]       = (void *) &sys_proc_start,

	[SYSCALL_INTR_INIT]        = (void *) &sys_intr_init,
	[SYSCALL_INTR_CONNECT]     = (void *) &sys_intr_connect,
	[SYSCALL_INTR_ACK]         = (void *) &sys_intr_ack,
	
	[SYSCALL_DEBUG]            = (void *) &sys_debug,
};

