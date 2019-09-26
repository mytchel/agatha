#include "head.h"
#include <sysnum.h>

/* TODO: 

	Endpoint pool should be per process and allocated
	by the process.

   */

#define MAX_ENDPOINTS 32
static endpoint_t endpoints[MAX_ENDPOINTS] = { 0 };
static size_t n_endpoints = 0;
static endpoint_t *free_endpoints = nil;
static size_t next_endpoint_id = 1;

endpoint_t *
endpoint_create(void)
{
	endpoint_t *e;

	if (free_endpoints == nil) {
		if (n_endpoints < MAX_ENDPOINTS) {
			free_endpoints = &endpoints[n_endpoints++];
			free_endpoints->next = nil;
		} else {
			debug_warn("out of endpoints!\n");
			return nil;
		}
	}

	e = free_endpoints;
	free_endpoints = e->next;

	e->id = next_endpoint_id++;

	e->next = nil;

	return e;
}

endpoint_t *
proc_create_endpoint_listener(proc_t *p)
{
	endpoint_t *e;

	e = endpoint_create();
	if (e == nil) {
		return nil;
	}

	e->mode = ENDPOINT_listen;
	e->listen.holder = p;
	e->listen.signal = 0;
	e->listen.waiting.head = nil;
	e->listen.waiting.tail = nil;

	e->next = p->listening;
	p->listening = e;

	debug_info("proc %i making listener endpoint %i\n",
		p->pid, e->id);

	return e;
}

endpoint_t *
proc_create_endpoint_connect(endpoint_t *o)
{
	endpoint_t *e;

	e = endpoint_create();
	if (e == nil) {
		return nil;
	}

	e->mode = ENDPOINT_connect;
	e->connect.other = o;

	debug_info("making connect eid %i to proc %i eid %i\n",
		e->id, o->listen.holder->pid, o->id);

	return e;
}

void
endpoint_log_waiting(endpoint_t *e)
{
	proc_t *p;

	debug_info("%i log endpoint %i\n", up->pid, e->id);
	debug_info("endpoint %i has signal 0x%x\n", e->id, e->listen.signal);

	for (p = e->listen.waiting.head; p != nil; p = p->wnext) {
		debug_info("endpoint %i proc %i state %i\n",
			e->id, p->pid, p->state);
	}
}

bool
endpoint_get_pending(endpoint_t *e, int *pid, uint8_t *m)
{
	proc_t *p;

	if (e->listen.signal != 0) {
		((uint32_t *) m)[0] = e->listen.signal;
		e->listen.signal = 0;
		*pid = PID_SIGNAL;
		return true;
	}

	for (p = e->listen.waiting.head; p != nil; p = p->wnext) {
		if (p->state == PROC_block_send) {
			memcpy(m, p->m, MESSAGE_LEN);

			if (p->offer != nil) {
				up->offer = p->offer;
				p->offer = nil;
			}

			p->state = PROC_block_reply;
			*pid = p->pid;
			return true;
		}
	}

	return false;
}

endpoint_t *
recv(endpoint_t *from, int *pid, uint8_t *m)
{
	endpoint_t *e;

	debug_info("%i recv\n", up->pid);

	if (from != nil && from->mode != ENDPOINT_listen) {
		debug_warn("proc %i tried to recv on non listener endpoint\n",
			up->pid);
		return nil;
	}

	while (true) {
		if (from != nil) {
			if (endpoint_get_pending(from, pid, m)) {
				return e;
			}
		} else {
			for (e = up->listening; e != nil; e = e->next) {
				if (endpoint_get_pending(e, pid, m)) {
					return e;
				}
			}
		}

		debug_info("nothing pending, schedule\n");

		up->recv_from = from;
		up->state = PROC_block_recv;
		schedule(nil);
	}
}

int
reply(endpoint_t *e, int pid, uint8_t *m)
{
	proc_t *p;

	if (e->mode != ENDPOINT_listen) {
		debug_warn("proc %i tried to reply on non listener endpoint %i\n",
			up->pid, e->id);
		return ERR;
	}

	debug_info("proc %i reply eid %i\n", up->pid, e->id);

	for (p = e->listen.waiting.head; p != nil; p = p->next) {
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
		e->listen.waiting.head = p->wnext;
	}

	if (p->wnext != nil) {
		p->wnext->wprev = p->wprev;
	} else {
		e->listen.waiting.tail = p->wprev;
	}

	memcpy(p->m, m, MESSAGE_LEN);

	if (up->offer != nil) {
		p->offer = up->offer;
		up->offer = nil;
	}

	proc_ready(p);
	schedule(p);
	
	return OK;
}

int
mesg(endpoint_t *e, uint8_t *rq, uint8_t *rp)
{
	endpoint_t *o;
	proc_t *p;

	if (e->mode != ENDPOINT_connect) {
		debug_warn("proc %i tried to mesg on non connect endpoint\n",
			up->pid);
		return ERR;
	}

	o = e->connect.other;
	p = o->listen.holder;

	if (p->state == PROC_free || p->state == PROC_fault) {
		debug_warn("endpoint holder %i in bad state %i\n", 
			p->pid, p->state);
		return ERR;
	}

	debug_info("proc %i send to e %i -> %i proc %i\n",
		up->pid, e->id, o->id, p->pid);
	
	memcpy(up->m, rq, MESSAGE_LEN);

	up->state = PROC_block_send;

	up->wprev = o->listen.waiting.tail;
	if (up->wprev != nil) {
		up->wprev->wnext = up;
	} else {
		o->listen.waiting.head = up;
	}

	up->wnext = nil;
	o->listen.waiting.tail = up;

	if (p->state == PROC_block_recv 
		&& (p->recv_from == nil || p->recv_from == o))
	{
		debug_info("wake them\n");
		proc_ready(p);
		schedule(p);
	} else {
		schedule(nil);
	}

	memcpy(rp, up->m, MESSAGE_LEN);

	return OK;
}

int
signal(endpoint_t *e, uint32_t s)
{
	endpoint_t *o;
	proc_t *p;

	debug_info("proc %i signal to 0x%x with 0x%x\n", up->pid, e->id, s);

	if (e->mode != ENDPOINT_connect) {
		debug_warn("proc %i tried to signal on non connect endpoint\n",
			up->pid);
		return ERR;
	}

	o = e->connect.other;
	p = o->listen.holder;

	o->listen.signal |= s;

	if (p->state == PROC_free || p->state == PROC_fault) {
		debug_warn("endpoint holder %i in bad state %i\n", 
			p->pid, p->state);
		return ERR;
	}

	if (p->state == PROC_block_recv 
		&& (p->recv_from == nil || p->recv_from == o))
	{
		debug_info("wake proc %i\n", p->pid);
		proc_ready(p);
		schedule(p);
	} 

	return OK;
}

endpoint_t *
proc_find_endpoint(endpoint_t *list, int eid)
{
	endpoint_t *e;

	for (e = list; e != nil; e = e->next) {
		if (e->id == eid) {
			return e;
		}
	}

	return nil;
}

int
sys_recv(int eid, int *pid, uint8_t *m)
{
	endpoint_t *e;

	if (eid == EID_ANY) {
		e = nil;
	} else {
		e = proc_find_endpoint(up->listening, eid);
		if (e == nil) {
			return ERR;
		}
	}

	e = recv(e, pid, m);
	if (e == nil) {
		return ERR;
	}

	return e->id;
}

int
sys_mesg(int eid, uint8_t *rq, uint8_t *rp)
{
	endpoint_t *e;

	e = proc_find_endpoint(up->sending, eid);
	if (e == nil) {
		return ERR;
	}

	return mesg(e, rq, rp);
}

int
sys_reply(int eid, int pid, uint8_t *m)
{
	endpoint_t *e;

	e = proc_find_endpoint(up->listening, eid);
	if (e == nil) {
		return ERR;
	}

	return reply(e, pid, m);
}

int
sys_signal(int eid, uint32_t s)
{
	endpoint_t *e;

	e = proc_find_endpoint(up->sending, eid);
	if (e == nil) {
		return ERR;
	}

	return signal(e, s);
}

	size_t
sys_yield(void)
{
	debug_info("yield %i in state %i\n", up->pid, up->state);

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
	endpoint_t *m, *e;
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

	e = proc_create_endpoint_connect(m);
	if (e == nil) {
		return ERR;
	}

	e->next = up->sending;
	up->sending = e;

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

	if (up->pid != 1) {
		debug_warn("proc %i is not root!\n", up->pid);
		return ERR;
	}

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
sys_endpoint_create(void)
{
	endpoint_t *e;

	e = proc_create_endpoint_listener(up);
	if (e == nil) {
		return ERR;
	}

	return e->id;
}

	size_t
sys_endpoint_offer(int eid)
{
	endpoint_t *e, *o;

	debug_info("proc %i wants to offer eid %i\n",
		up->pid, eid);

	o = proc_find_endpoint(up->listening, eid);
	if (o != nil) {
		e = proc_create_endpoint_connect(o);
		if (e == nil) {
			return ERR;
		}

		e->next = nil;
		up->offer = e;
		return OK;
	}
	
	o = proc_find_endpoint(up->sending, eid);
	if (o != nil) {
		e = proc_create_endpoint_connect(o->connect.other);
		if (e == nil) {
			return ERR;
		}

		e->next = nil;
		up->offer = e;
		return OK;
	}

	return ERR;
}

endpoint_t *
endpoint_accept(void)
{
	endpoint_t *e;

	e = up->offer;
	if (e == nil) {
		debug_info("proc %i tried to accept but nothing was offered\n",
			up->pid); 

		return nil;
	}

	up->offer = nil;

	e->next = up->sending;
	up->sending = e;

	debug_info("proc %i accepting connection eid %i\n",
			up->pid, e->id); 

	return e;
}

	size_t
sys_endpoint_accept(void)
{
	endpoint_t *e;

	e = endpoint_accept();
	if (e == nil) {
		return ERR;
	}

	return e->id;
}

	size_t
sys_intr_register(int irqn, int eid, uint32_t signal)
{
	endpoint_t *o, *e;
		
	debug_info("%i intr register irq %i eid %i\n", up->pid, irqn, eid);

	o = proc_find_endpoint(up->listening, eid);
	if (o == nil) {
		debug_warn("%i endpoint %i not found\n", up->pid, eid);
		return ERR;
	}

	e = proc_create_endpoint_connect(o);
	if (e == nil) {
		return ERR;
	}

	e->next = nil;

	return irq_add_user(irqn, e, signal);
}

	size_t
sys_intr_ack(int irqn)
{
	debug_info("%i called sys intr_ack\n", up->pid);

	irq_ack(irqn);

	return OK;
}

	size_t
sys_debug(char *m)
{
	debug_warn("%i debug %s\n", up->pid, m);

	return OK;
}

void *systab[NSYSCALLS] = {
	[SYSCALL_YIELD]            = (void *) &sys_yield,
	[SYSCALL_MESG]             = (void *) &sys_mesg,
	[SYSCALL_RECV]             = (void *) &sys_recv,
	[SYSCALL_REPLY]            = (void *) &sys_reply,
	[SYSCALL_SIGNAL]           = (void *) &sys_signal,
	[SYSCALL_PID]              = (void *) &sys_pid,
	[SYSCALL_EXIT]             = (void *) &sys_exit,
	[SYSCALL_PROC_NEW]         = (void *) &sys_proc_new,
	[SYSCALL_PROC_SETUP]       = (void *) &sys_proc_setup,
	[SYSCALL_ENDPOINT_CREATE]  = (void *) &sys_endpoint_create,
	[SYSCALL_ENDPOINT_OFFER]   = (void *) &sys_endpoint_offer,
	[SYSCALL_ENDPOINT_ACCEPT]  = (void *) &sys_endpoint_accept,
	[SYSCALL_INTR_REGISTER]    = (void *) &sys_intr_register,
	[SYSCALL_INTR_ACK]         = (void *) &sys_intr_ack,
	[SYSCALL_DEBUG]            = (void *) &sys_debug,
};

