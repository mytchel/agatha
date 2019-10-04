#include "head.h"
#include <sysnum.h>

/* TODO: 

	Endpoint pool should be per process and allocated
	by the process.

   */

#define MAX_CAP 64
static cap_t caps[MAX_CAP] = { 0 };
static size_t n_caps = 0;
static cap_t *free_caps = nil;

#define MAX_OBJ 32
static obj_untyped_t objs[MAX_OBJ] = { 0 };

int
cap_offer(int cid);

cap_t *
cap_accept(void);

cap_t *
cap_create(proc_t *p)
{
	cap_t *c;

	if (free_caps == nil) {
		if (n_caps < MAX_CAP) {
			free_caps= &caps[n_caps++];
			free_caps->next = nil;
		} else {
			debug_warn("out of capabilities!\n");
			return nil;
		}
	}

	c = free_caps;
	free_caps = c->next;

	c->id = ++p->next_cap_id;

	c->next = p->caps;
	p->caps = c;

	return c;
}

obj_untyped_t *
obj_create(void)
{
	size_t i;

	for (i = 0; i < MAX_OBJ; i++) {
		if (objs[i].h.type == OBJ_untyped) {
			return (void *) &objs[i];
		}
	}

	debug_warn("out of objs!\n");
	return nil;
}

cap_t *
proc_endpoint_create(proc_t *p)
{
	obj_endpoint_t *e;
	cap_t *c;

	e = (obj_endpoint_t *) obj_create();
	if (e == nil) {
		return nil;
	}

	c = cap_create(p);
	if (c == nil) {
		return nil;
	}

	e->h.refs = 1;
	e->h.type = OBJ_endpoint;

	e->holder = p;
	e->signal = 0;
	e->waiting.head = nil;
	e->waiting.tail = nil;

	c->perm = CAP_read;
	c->obj = (obj_untyped_t *) e;

	debug_info("proc %i making endpoint 0x%x, cap %i\n", p->pid, e, c->id);

	return c;
}

cap_t *
proc_endpoint_connect(proc_t *p, obj_endpoint_t *e)
{
	cap_t *c;

	c = cap_create(p);
	if (c == nil) {
		return nil;
	}

	c->perm = CAP_write;
	c->obj = (obj_untyped_t *) e;

	debug_info("proc %i connect endpoint 0x%x, cap %i\n",
		p->pid, e, c->id);

	return c;
}

cap_t *
proc_find_cap(proc_t *p, int cid)
{
	cap_t *c;

	for (c = p->caps; c != nil; c = c->next) {
		if (c->id == cid) {
			return c;
		}
	}

	return nil;
}

bool
endpoint_get_pending(obj_endpoint_t *e, int *pid, uint8_t *m, int *cid)
{
	proc_t *p;

	if (e->signal != 0) {
		((uint32_t *) m)[0] = e->signal;
		e->signal = 0;
		*pid = PID_SIGNAL;
		if (cid != nil) *cid = 0;
		return true;
	}

	for (p = e->waiting.head; p != nil; p = p->wnext) {
		if (p->state == PROC_block_send) {
			memcpy(m, p->m, MESSAGE_LEN);

			if (p->give.obj != nil) {
				up->take.obj = p->give.obj;
				up->take.perm = p->give.perm;

				p->give.obj = 0;
				p->give.perm = 0;
			}

			if (cid != nil) {
 				cap_t *c = cap_accept();

				if (c != nil) {
					debug_info("%i recv got cap id %i\n", up->pid, *cid);
					*cid = c->id;
				} else {
					debug_warn("%i recv didn't get cap\n", up->pid);
					*cid = 0;
				}
			}

			p->state = PROC_block_reply;
			*pid = p->pid;
			return true;
		}
	}

	return false;
}

cap_t *
recv(cap_t *from, int *pid, uint8_t *m, int *cid)
{
	cap_t *c;

	while (true) {
		if (from != nil) {
			if (endpoint_get_pending((obj_endpoint_t *) from->obj, pid, m, cid)) {
				return from;
			}
			
			up->recv_from = (obj_endpoint_t *) from->obj;
		} else {
			for (c = up->caps; c != nil; c = c->next) {
				if ((c->perm & CAP_read) && c->obj->h.type == OBJ_endpoint) {
					if (endpoint_get_pending((obj_endpoint_t *) c->obj, pid, m, cid)) {
						return c;
					}
				}
			}

			up->recv_from = nil;
		}

		debug_info("nothing pending, schedule\n");

		up->state = PROC_block_recv;
		schedule(nil);
	}
}

int
reply(obj_endpoint_t *e, int pid, uint8_t *m, int cid)
{
	proc_t *p;

	for (p = e->waiting.head; p != nil; p = p->wnext) {
		if (p->state == PROC_block_reply && p->pid == pid) {
			break;
		}
	}

	if (p == nil) {
		debug_warn("proc %i tried to reply waiting proc %i not found\n",
			up->pid, pid);
		return ERR;
	}

	if (p->wprev != nil) {
		p->wprev->wnext = p->wnext;
	} else {
		e->waiting.head = p->wnext;
	}

	if (p->wnext != nil) {
		p->wnext->wprev = p->wprev;
	} else {
		e->waiting.tail = p->wprev;
	}

	memcpy(p->m, m, MESSAGE_LEN);

	if (cid != 0) {
		debug_info("%i offer cap id %i\n", up->pid, cid);
		cap_offer(cid);
	}

	if (up->give.obj != nil) {
		p->take.obj = up->give.obj;
		p->take.perm = up->give.perm;

		up->give.obj = 0;
		up->give.perm = 0;
	}

	proc_ready(p);
	schedule(p);
	
	return OK;
}

int
mesg(obj_endpoint_t *e, uint8_t *rq, uint8_t *rp, int *cid)
{
	proc_t *p;

	p = e->holder;

	if (p->state == PROC_free || p->state == PROC_fault) {
		debug_warn("endpoint holder %i in bad state %i\n", 
			p->pid, p->state);
		return ERR;
	}

	memcpy(up->m, rq, MESSAGE_LEN);

	if (cid != nil && *cid != 0) {
		debug_info("%i offer cap id %i\n", up->pid, *cid);
		cap_offer(*cid);
	}

	up->state = PROC_block_send;

	up->wprev = e->waiting.tail;
	if (up->wprev != nil) {
		up->wprev->wnext = up;
	} else {
		e->waiting.head = up;
	}

	up->wnext = nil;
	e->waiting.tail = up;

	if (p->state == PROC_block_recv 
		&& (p->recv_from == nil || p->recv_from == e))
	{
		debug_info("wake %i\n", p->pid);
		proc_ready(p);
		schedule(p);
	} else {
		debug_info("%i is not waiting, state %i\n", 
			p->pid, p->state);
		schedule(nil);
	}

	memcpy(rp, up->m, MESSAGE_LEN);

	if (cid != nil) {
		cap_t *c = cap_accept();
		if (c != nil) {
			debug_info("%i mesg got cap id %i\n", up->pid, *cid);
			*cid = c->id;
		} else {
			debug_warn("%i mesg didn't get cap\n", up->pid);
			*cid = 0;
		}
	}

	return OK;
}

int
signal(obj_endpoint_t *e, uint32_t s)
{
	proc_t *p;

	p = e->holder;

	e->signal |= s;

	if (p->state == PROC_free || p->state == PROC_fault) {
		debug_warn("endpoint holder %i in bad state %i\n", 
			p->pid, p->state);
		return ERR;
	}

	if (p->state == PROC_block_recv 
		&& (p->recv_from == nil || p->recv_from == e))
	{
		debug_warn("wake proc %i\n", p->pid);
		proc_ready(p);
		schedule(p);
	} else {
		debug_warn("signal proc %i not waiting\n", p->pid);
	}

	return OK;
}

int
sys_recv(int from, int *pid, uint8_t *m, int *cid)
{
	cap_t *c;

	debug_info("%i recv %i\n", up->pid, cid);

	if (from == EID_ANY) {
		c = nil;
	} else {
		c = proc_find_cap(up, from);
		if (c == nil) {
			return ERR;
		} else if (!(c->perm & CAP_read) || c->obj->h.type != OBJ_endpoint) {
			return ERR;
		}
	}

	c = recv(c, pid, m, cid);
	if (c == nil) {
		return ERR;
	}

	return c->id;
}

int
sys_mesg(int to, uint8_t *rq, uint8_t *rp, int *cid)
{
	cap_t *c;

	debug_info("%i mesg %i\n", up->pid, to);

	c = proc_find_cap(up, to);
	if (c == nil) {
		return ERR;
	} else if (!(c->perm & CAP_write) || c->obj->h.type != OBJ_endpoint) {
		return ERR;
	}

	return mesg((obj_endpoint_t *) c->obj, rq, rp, cid);
}

int
sys_reply(int to, int pid, uint8_t *m, int cid, int test)
{
	cap_t *c;

	debug_info("%i reply %i pid %i, give cap %i\n", up->pid, to, pid, cid);

	c = proc_find_cap(up, to);
	if (c == nil) {
		return ERR;
	} else if (!(c->perm & CAP_read) || c->obj->h.type != OBJ_endpoint) {
		return ERR;
	}

	return reply((obj_endpoint_t *) c->obj, pid, m, cid);
}

int
sys_signal(int to, uint32_t s)
{
	cap_t *c;

	debug_info("%i signal %i 0x%x\n", up->pid, to, s);

	c = proc_find_cap(up, to);
	if (c == nil) {
		return ERR;
	} else if (!(c->perm & CAP_write) || c->obj->h.type != OBJ_endpoint) {
		return ERR;
	}

	return signal((obj_endpoint_t *) c->obj, s);
}

int
cap_offer(int cid)
{
	cap_t *c;

	c = proc_find_cap(up, cid);
	if (c == nil) {
		return ERR;
	}
	
	debug_info("%i offer obj 0x%x\n", up->pid, c->obj);

	up->give.perm = c->perm;
	up->give.obj = c->obj;

	return OK;
}

cap_t *
cap_accept(void)
{
	cap_t *c;

	if (up->take.obj == nil) {
		return nil;
	}

	c = cap_create(up);
	if (c == nil) {
		return nil;
	}

	c->perm = up->take.perm;
	c->obj = up->take.obj;

	up->take.perm = 0;
	up->take.obj = nil;

	return c;
}

	size_t
sys_cap_offer(int cid)
{
	return cap_offer(cid);
}

	size_t
sys_cap_accept(void)
{
	cap_t *c;

	debug_info("%i accept obj 0x%x\n", up->pid, up->take.obj);

	c = cap_accept();
	if (c == nil) {
		return ERR;
	}

	return c->id;
}

	size_t
sys_endpoint_create(void)
{
	cap_t *c;

	c = proc_endpoint_create(up);
	if (c == nil) {
		return ERR;
	}

	debug_info("%i create endpoint obj 0x%x, cap id %i\n", up->pid, c->obj, c->id);

	return c->id;
}

	size_t
sys_endpoint_connect(int cid)
{
	obj_endpoint_t *l;
	cap_t *c, *n;

	c = proc_find_cap(up, cid);
	if (c == nil) {
		return ERR;
	} else if (c->obj->h.type != OBJ_endpoint) {
		return ERR;
	} else {
		l = (obj_endpoint_t *) c->obj;
	}

	n = proc_endpoint_connect(up, l);
	if (n == nil) {
		return ERR;
	}
	
	debug_info("%i connect endpoint obj 0x%x, cap id %i\n", up->pid, c->obj, n->id);

	return n->id;
}

	size_t
sys_intr_create(int irqn)
{
	obj_intr_t *i;
	cap_t *c;

	if (up->pid != ROOT_PID) {
		return ERR;
	}

	i = (obj_intr_t *) obj_create();
	if (i == nil) {
		return ERR;
	}

	c = cap_create(up);
	if (c == nil) {
		return ERR;
	}

	if (!intr_cap_claim(irqn)) {
		debug_warn("intr cap claim failed, TODO free resources\n");
		return ERR;
	}

	i->h.refs = 1;
	i->h.type = OBJ_intr;

	i->irqn = irqn;
	i->end = nil;
	i->signal = 0;

	c->perm = CAP_write;
	c->obj = (obj_untyped_t *) i;

	debug_info("%i create intr obj 0x%x, cap id %i irqn %i\n", 
		up->pid, c->obj, c->id, irqn);

	return c->id;
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
	} else if (i->obj->h.type != OBJ_intr) {
		return ERR;
	} else if (e->obj->h.type != OBJ_endpoint || !(e->perm & CAP_read)) {
		return ERR;
	}

	oi = (obj_intr_t *) i->obj;
	oe = (obj_endpoint_t *) e->obj;

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
	} else if (c->obj->h.type != OBJ_intr) {
		return ERR;
	}

	i = (obj_intr_t *) c->obj;

	irq_ack(i->irqn);

	return OK;
}

	size_t
sys_yield(void)
{
	debug_info("%i yield\n", up->pid);

	schedule(nil);

	return OK;
}

	size_t
sys_pid(void)
{
	debug_info("%i get pid\n", up->pid);

	return up->pid;
}

	size_t
sys_exit(uint32_t code)
{
	debug_warn("%i proc exiting with 0x%x\n", up->pid, code);
	/*
	union proc_msg m;

	m.exit.type = PROC_exit_msg;
	m.exit.code = code;
*/
	proc_fault(up);

	/*mesg_supervisor((uint8_t *) &m);*/

	schedule(nil);

	panic("schedule returned to exit!\n");

	return ERR;
}

	size_t
sys_proc_new(size_t vspace, int priority, int *p_id, int *e_id)
{
	cap_t *m, *e;
	proc_t *p;

	debug_info("%i proc new\n", up->pid);

	p = proc_new(vspace, priority);
	if (p == nil) {
		debug_warn("proc_new failed\n");
		return ERR;
	}

	m = proc_endpoint_create(p);
	if (m == nil) {
		return ERR;
	}

	e = proc_endpoint_connect(up, (obj_endpoint_t *) m->obj);
	if (e == nil) {
		return ERR;
	}

	debug_info("new proc %i with vspace 0x%x\n", 
			p->pid, vspace);

	func_label(&p->label, 
			(size_t) p->kstack, KSTACK_LEN,
			(size_t) &proc_start);

	proc_ready(p);

	*p_id = p->pid;
	*e_id = e->id;

	return OK;
}

	size_t
sys_proc_setup(int pid, procstate_t state)
{
	proc_t *p;

	p = find_proc(pid);
	if (p == nil) {
		return ERR;
	}

	if (state == PROC_free) {
		debug_info("%i putting proc %i into state free\n", up->pid, p->pid);
		return proc_free(p);

	} else if (state == PROC_fault) {
		debug_info("%i putting proc %i into state fault\n", up->pid, p->pid);

		return proc_fault(p);

	} else if (state == PROC_ready) {
		debug_info("%i putting proc %i into state setup\n", up->pid, p->pid);

		return proc_ready(p);

	} else {
		return ERR;
	}
}

	size_t
sys_debug(char *m)
{
	debug_warn("%i debug %s\n", up->pid, m);

	return OK;
}

void *systab[NSYSCALLS] = {
	[SYSCALL_YIELD]            = (void *) &sys_yield,
	[SYSCALL_PID]              = (void *) &sys_pid,
	[SYSCALL_EXIT]             = (void *) &sys_exit,
	[SYSCALL_PROC_NEW]         = (void *) &sys_proc_new,
	[SYSCALL_PROC_SETUP]       = (void *) &sys_proc_setup,
	[SYSCALL_MESG]             = (void *) &sys_mesg,
	[SYSCALL_RECV]             = (void *) &sys_recv,
	[SYSCALL_REPLY]            = (void *) &sys_reply,
	[SYSCALL_SIGNAL]           = (void *) &sys_signal,
	[SYSCALL_CAP_OFFER]        = (void *) &sys_cap_offer,
	[SYSCALL_CAP_ACCEPT]       = (void *) &sys_cap_accept,
	[SYSCALL_ENDPOINT_CREATE]  = (void *) &sys_endpoint_create,
	[SYSCALL_ENDPOINT_CONNECT] = (void *) &sys_endpoint_connect,
	[SYSCALL_INTR_CREATE]      = (void *) &sys_intr_create,
	[SYSCALL_INTR_CONNECT]     = (void *) &sys_intr_connect,
	[SYSCALL_INTR_ACK]         = (void *) &sys_intr_ack,
	[SYSCALL_DEBUG]            = (void *) &sys_debug,
};

