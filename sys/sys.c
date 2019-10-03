#include "head.h"
#include <sysnum.h>

/* TODO: 

	Endpoint pool should be per process and allocated
	by the process.

   */

#define MAX_CAPS 32
static capability_t caps[MAX_CAPS] = { 0 };
static size_t n_caps = 0;
static capability_t *free_caps = nil;
static size_t next_cap_id = 1;

capability_t *
capability_create(void)
{
	capability_t *c;

	if (free_caps == nil) {
		if (n_caps < MAX_CAPS) {
			free_caps= &caps[n_caps++];
			free_caps->next = nil;
		} else {
			debug_warn("out of capabilities!\n");
			return nil;
		}
	}

	c = free_caps;
	free_caps = c->next;

	c->id = next_cap_id++;

	c->next = nil;

	return c;
}

capability_t *
proc_create_endpoint_listener(proc_t *p)
{
	capability_t *c;

	c = capability_create();
	if (c == nil) {
		return nil;
	}

	c->type = CAP_endpoint_listen;
	c->c.listen.holder = p;
	c->c.listen.signal = 0;
	c->c.listen.waiting.head = nil;
	c->c.listen.waiting.tail = nil;

	c->next = p->caps;
	p->caps = c;

	debug_info("proc %i making listener endpoint %i\n",
		p->pid, c->id);

	return c;
}

capability_t *
proc_create_endpoint_connect(proc_t *p, endpoint_listen_t *l)
{
	capability_t *c;

	c = capability_create();
	if (c == nil) {
		return nil;
	}

	c->type = CAP_endpoint_connect;
	c->c.connect.other = l;

	c->next = p->caps;
	p->caps = c;

	debug_info("making connect eid %i to proc %i\n",
		c->id, l->holder->pid);

	return c;
}

capability_t *
proc_find_capability(proc_t *p, int cid)
{
	capability_t *c;

	for (c = p->caps; c != nil; c = c->next) {
		if (c->id == cid) {
			return c;
		}
	}

	return nil;
}

bool
endpoint_get_pending(capability_t *c, int *pid, uint8_t *m)
{
	endpoint_listen_t *l = &c->c.listen;
	proc_t *p;

	if (l->signal != 0) {
		((uint32_t *) m)[0] = l->signal;
		l->signal = 0;
		*pid = PID_SIGNAL;
		return true;
	}

	for (p = l->waiting.head; p != nil; p = p->wnext) {
		if (p->state == PROC_block_send) {
			memcpy(m, p->m, MESSAGE_LEN);

			if (p->offering != nil) {
				up->offered = p->offering;
				p->offering = nil;
			}

			p->state = PROC_block_reply;
			*pid = p->pid;
			return true;
		}
	}

	return false;
}

capability_t *
recv(capability_t *from, int *pid, uint8_t *m)
{
	capability_t *c;

	if (from != nil && from->type != CAP_endpoint_listen) {
		return nil;
	}

	while (true) {
		if (from != nil) {
			if (endpoint_get_pending(from, pid, m)) {
				return from;
			}
			
			up->recv_from = &from->c.listen;
		} else {
			for (c = up->caps; c != nil; c = c->next) {
				if (c->type == CAP_endpoint_listen) {
					if (endpoint_get_pending(c, pid, m)) {
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
reply(endpoint_listen_t *l, int pid, uint8_t *m)
{
	proc_t *p;

	for (p = l->waiting.head; p != nil; p = p->wnext) {
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
		l->waiting.head = p->wnext;
	}

	if (p->wnext != nil) {
		p->wnext->wprev = p->wprev;
	} else {
		l->waiting.tail = p->wprev;
	}

	memcpy(p->m, m, MESSAGE_LEN);

	if (up->offering != nil) {
		p->offered = up->offering;
		up->offering = nil;
	}

	proc_ready(p);
	schedule(p);
	
	return OK;
}

int
mesg(endpoint_listen_t *l, uint8_t *rq, uint8_t *rp)
{
	proc_t *p;

	p = l->holder;

	if (p->state == PROC_free || p->state == PROC_fault) {
		debug_warn("endpoint holder %i in bad state %i\n", 
			p->pid, p->state);
		return ERR;
	}

	memcpy(up->m, rq, MESSAGE_LEN);

	up->state = PROC_block_send;

	up->wprev = l->waiting.tail;
	if (up->wprev != nil) {
		up->wprev->wnext = up;
	} else {
		l->waiting.head = up;
	}

	up->wnext = nil;
	l->waiting.tail = up;

	if (p->state == PROC_block_recv 
		&& (p->recv_from == nil || p->recv_from == l))
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

	return OK;
}

int
signal(endpoint_listen_t *l, uint32_t s)
{
	proc_t *p;

	p = l->holder;

	l->signal |= s;

	if (p->state == PROC_free || p->state == PROC_fault) {
		debug_warn("endpoint holder %i in bad state %i\n", 
			p->pid, p->state);
		return ERR;
	}

	if (p->state == PROC_block_recv 
		&& (p->recv_from == nil || p->recv_from == l))
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
sys_recv(int cid, int *pid, uint8_t *m)
{
	capability_t *c;

	debug_info("%i recv %i\n", up->pid, cid);

	if (cid == EID_ANY) {
		c = nil;
	} else {
		c = proc_find_capability(up, cid);
		if (c == nil) {
			return ERR;
		} else if (c->type != CAP_endpoint_listen) {
			return ERR;
		}
	}

	c = recv(c, pid, m);
	if (c == nil) {
		return ERR;
	}

	return c->id;
}

int
sys_mesg(int cid, uint8_t *rq, uint8_t *rp)
{
	capability_t *c;

	debug_info("%i mesg %i\n", up->pid, cid);

	c = proc_find_capability(up, cid);
	if (c == nil) {
		return ERR;
	} else if (c->type != CAP_endpoint_connect) {
		return ERR;
	}

	return mesg(c->c.connect.other, rq, rp);
}

int
sys_reply(int cid, int pid, uint8_t *m)
{
	capability_t *c;

	debug_info("%i reply %i pid %i\n", up->pid, cid, pid);

	c = proc_find_capability(up, cid);
	if (c == nil) {
		return ERR;
	} else if (c->type != CAP_endpoint_listen) {
		return ERR;
	}

	return reply(&c->c.listen, pid, m);
}

int
sys_signal(int cid, uint32_t s)
{
	capability_t *c;

	debug_info("%i signal %i 0x%x\n", up->pid, cid, s);

	c = proc_find_capability(up, cid);
	if (c == nil) {
		return ERR;
	} else if (c->type != CAP_endpoint_connect) {
		return ERR;
	}

	return signal(c->c.connect.other, s);
}

	size_t
sys_cap_offer(int cid)
{
	capability_t **c, *n;

	debug_info("proc %i wants to offer cid %i\n",
		up->pid, cid);

	for (c = &up->caps; *c != nil; c = &(*c)->next) {
		if ((*c)->id == cid) {
			break;
		}
	}

	if (*c == nil) {
		return ERR;
	}

	n = *c;
	*c = n->next;

	n->next = up->offering;
	up->offering = n;
	
	return OK;
}

capability_t *
capability_accept(void)
{
	capability_t *c;

	c = up->offered;

	if (c == nil) {
		return nil;
	}
	
	up->offered = c->next;

	c->next = up->caps;
	up->caps = c;

	return c;
}

	size_t
sys_cap_accept(void)
{
	capability_t *c;

	c = capability_accept();
	if (c == nil) {
		return ERR;
	}

	return c->id;
}

	size_t
sys_endpoint_create(void)
{
	capability_t *c;

	c = proc_create_endpoint_listener(up);
	if (c == nil) {
		return ERR;
	}

	return c->id;
}

	size_t
sys_endpoint_connect(int cid)
{
	capability_t *c, *n;
	endpoint_listen_t *l;

	c = proc_find_capability(up, cid);
	if (c == nil) {
		return ERR;
	}
	
	if (c->type == CAP_endpoint_listen) {
		l = &c->c.listen;
	} else if (c->type == CAP_endpoint_connect) {
		l = c->c.connect.other;
	} else {
		return ERR;
	}

	n = proc_create_endpoint_connect(up, l);
	if (n == nil) {
		return ERR;
	}

	return n->id;
}

	size_t
sys_intr_create(int irqn)
{
	capability_t *c;

	if (up->pid != ROOT_PID) {
		return ERR;
	}

	c = get_user_int_cap(irqn);
	if (c == nil) {
		return ERR;
	}

	c->id = next_cap_id++;

	c->next = up->caps;
	up->caps = c;

	return c->id;
}

	size_t
sys_intr_connect(int iid, int eid, uint32_t signal)
{
	capability_t *i, *e;

	debug_info("%i intr connect %i to %i\n", up->pid, iid, eid);

	i = proc_find_capability(up, iid);
	e = proc_find_capability(up, eid);

	if (i == nil || e == nil) {
		return ERR;
	} else if (i->type != CAP_interrupt) {
		return ERR;
	} else if (e->type != CAP_endpoint_listen) {
		return ERR;
	}

	i->c.interrupt.other = &e->c.listen;
	i->c.interrupt.signal = signal;

	irq_enable(i->c.interrupt.irqn);
	
	return OK;
}

	size_t
sys_intr_ack(int iid)
{
	capability_t *i;

	debug_info("%i intr ack %i\n", up->pid, iid);

	i = proc_find_capability(up, iid);
	if (i == nil) {
		return ERR;
	} else if (i->type != CAP_interrupt) {
		return ERR;
	}

	irq_ack(i->c.interrupt.irqn);

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
	capability_t *m, *e;
	proc_t *p;

	debug_info("%i proc new\n", up->pid);

	p = proc_new(vspace, priority);
	if (p == nil) {
		debug_warn("proc_new failed\n");
		return ERR;
	}

	m = proc_create_endpoint_listener(p);
	if (m == nil) {
		return ERR;
	}

	e = proc_create_endpoint_connect(up, &m->c.listen);
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

