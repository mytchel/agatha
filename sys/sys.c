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

endpoint_t *
endpoint_add(proc_t *p)
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

	e->id = p->next_endpoint_id++;

	e->next = p->endpoints;
	p->endpoints = e;

	return e;
}

endpoint_t *
proc_add_endpoint_listener(proc_t *p)
{
	endpoint_t *e;

	e = endpoint_add(p);
	if (e == nil) {
		return nil;
	}

	e->mode = ENDPOINT_listen;
	e->listen.holder = p;
	e->listen.signal = 0;
	e->listen.waiting.head = nil;
	e->listen.waiting.tail = nil;

	debug_info("proc %i making listener endpoint %i for %i\n",
		up->pid, e->id, p->pid);

	return e;
}

endpoint_t *
proc_add_endpoint_connect(proc_t *p, endpoint_t *o)
{
	endpoint_t *e;

	e = endpoint_add(p);
	if (e == nil) {
		return nil;
	}

	e->mode = ENDPOINT_connect;
	e->connect.other = o;

	debug_info("proc %i making connect proc %i endpoint %i to proc %i endpoint %i\n",
		up->pid, p->pid, e->id, o->listen.holder->pid, o->id);

	return e;
}

endpoint_t *
proc_find_endpoint(proc_t *p, int eid)
{
	endpoint_t *e;

	for (e = p->endpoints; e != nil; e = e->next) {
		if (e->id == eid) {
			return e;
		}
	}

	return nil;
}

bool
endpoint_get_pending(endpoint_t *e, int *pid, uint8_t *m)
{
	proc_t *p;

	debug_info("%i checking %i\n", up->pid, e->id);
	debug_info("%i signal %i = 0x%x\n", up->pid, e->id, e->listen.signal);

	if (e->listen.signal != 0) {
		((uint32_t *) m)[0] = e->listen.signal;
		e->listen.signal = 0;
		*pid = PID_NONE;
		return true;
	}

	debug_info("%i check list %i\n", up->pid, e->id);
	for (p = e->listen.waiting.head; p != nil; p = p->wnext) {
		debug_info("%i list %i proc %i in state %i\n", up->pid, e->id,
			p->pid, p->state);

		if (p->state == PROC_block_send) {
			debug_info("ok\n");
			memcpy(m, p->m, MESSAGE_LEN);
			p->state = PROC_block_reply;
			*pid = p->pid;
			return true;
		}
	}

	return false;
}

	int
recv(endpoint_t *from, int *pid, uint8_t *m)
{
	endpoint_t *e;

	debug_info("%i recv\n", up->pid);

	if (from != nil && from->mode != ENDPOINT_listen) {
		debug_warn("proc %i tried to recv on non listener endpoint\n",
			up->pid);
		return ERR;
	}

	while (true) {
		if (from != nil) {
			debug_info("%i check from %i\n", up->pid, from->id);
			if (endpoint_get_pending(from, pid, m)) {
				return from->id;
			}
		} else {
			for (e = up->endpoints; e != nil; e = e->next) {
				if (endpoint_get_pending(e, pid, m)) {
					return e->id;
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

	p = find_proc(pid);
	if (p == nil) {
		return ERR;
	} else if (e->mode != ENDPOINT_listen) {
		debug_warn("proc %i tried to reply on non listener endpoint %i\n",
			up->pid, e->id);
		return ERR;
	} else if (p->state != PROC_block_reply) {
		debug_warn("proc %i tried to reply but proc %i not blocked?\n",
			up->pid, p->pid);
		return ERR;
	} else if (p->wlist != &e->listen.waiting) {
		debug_warn("proc %i tried to reply but proc %i is not in %i's wait list\n",
			up->pid, p->pid, e->id);
		return ERR;
	}

	if (p->wprev != nil) {
		p->prev->wnext = p->wnext;
	} else {
		e->listen.waiting.head = p->wnext;
	}

	if (p->wnext != nil) {
		p->next->prev = p->prev;
	} else {
		e->listen.waiting.tail = p->prev;
	}

	if (p->state == PROC_block_reply) {
		proc_ready(p);
		schedule(p);
	} else {
		debug_warn("waiting proc %i in bad state %i\n",
			p->pid, p->state);
	}
	
	return OK;
}

int
mesg(endpoint_t *e, uint8_t *m)
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

	memcpy(up->m, m, MESSAGE_LEN);

	up->state = PROC_block_send;

	up->wnext = nil;
	up->wprev = o->listen.waiting.tail;
	o->listen.waiting.tail = up;
	if (o->listen.waiting.head == nil) {
		o->listen.waiting.head = up;
	}

	debug_info("added to list\n");

	up->wlist = &o->listen.waiting;

	if (p->state == PROC_block_recv 
		&& (p->recv_from == nil || p->recv_from == o))
	{
		debug_info("wake them\n");
		proc_ready(p);
		schedule(p);
	} else {
		schedule(nil);
	}

	return OK;
}

int
signal(endpoint_t *e, uint32_t s)
{
	endpoint_t *o;
	proc_t *p;

	if (e->mode != ENDPOINT_connect) {
		debug_warn("proc %i tried to signal on non connect endpoint\n",
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

	if (p->state == PROC_block_recv 
		&& (p->recv_from == nil || p->recv_from == o))
	{
		proc_ready(p);
	} 

	return OK;
}

int
sys_recv(int eid, int *pid, uint8_t *m)
{
	endpoint_t *e;

	if (eid == EID_ANY) {
		e = nil;
	} else {
		e = proc_find_endpoint(up, eid);
		if (e == nil) {
			return ERR;
		}
	}

	return recv(e, pid, m);
}

int
sys_mesg(int eid, uint8_t *m)
{
	endpoint_t *e;

	e = proc_find_endpoint(up, eid);
	if (e == nil) {
		return ERR;
	}

	return mesg(e, m);
}

int
sys_reply(int eid, int pid, uint8_t *m)
{
	endpoint_t *e;

	e = proc_find_endpoint(up, eid);
	if (e == nil) {
		return ERR;
	}

	return reply(e, pid, m);
}

int
sys_signal(int eid, uint32_t s)
{
	endpoint_t *e;

	e = proc_find_endpoint(up, eid);
	if (e == nil) {
		return ERR;
	}

	return signal(e, s);
}

	size_t
sys_yield(void)
{
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
sys_proc_new(int supervisor_pid, 
		int supervisor_eid,
		size_t vspace, 
		int priority)
{
	endpoint_t *e, *o;
	proc_t *p, *s;

	debug_info("%i proc new\n", up->pid);

	if (up->pid != 1) {
		debug_warn("proc %i is not proc1!\n", up->pid);
		return ERR;
	}

	s = find_proc(supervisor_pid);
	if (s == nil) {
		debug_warn("couldn't find pid %i for supervisor\n",
			supervisor_pid);
		return ERR;
	}

	o = proc_find_endpoint(s, supervisor_eid);
	if (o == nil) {
		debug_warn("couldn't find endpoint %i for supervisor %i\n",
			supervisor_eid, s->pid);
		return ERR;
	}

	p = proc_new(vspace, priority);
	if (p == nil) {
		debug_warn("proc_new failed\n");
		return ERR;
	}

	e = proc_add_endpoint_listener(p);
	if (e == nil) {
		debug_warn("error creating main endpoint\n");
		return ERR;
	}

	e = proc_add_endpoint_connect(p, o);
	if (e == nil) {
		debug_warn("error creating supervisor endpoint\n");
		return ERR;
	}

	debug_info("new proc %i with supervisor %i vspace 0x%x\n", 
			p->pid, supervisor_pid, vspace);

	func_label(&p->label, (size_t) p->kstack, KSTACK_LEN,
			(size_t) &proc_start);

	proc_ready(p);

	return p->pid;
}

	size_t
sys_proc_setup(int pid, procstate_t state)
{
	proc_t *p;

	if (up->pid != 1) {
		debug_warn("proc %i is not proc1!\n", up->pid);
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
sys_endpoint_listen(int pid)
{
	endpoint_t *e;
	proc_t *p;

	if (up->pid != 1) {
		debug_warn("proc %i is not proc1!\n", up->pid);
		return ERR;
	}

	p = find_proc(pid);
	if (p == nil) {
		return ERR;
	}

	e = proc_add_endpoint_listener(p);
	if (e == nil) {
		return ERR;
	}

	return e->id;
}

	size_t
sys_endpoint_connect(int from, int to, int to_eid)
{
	proc_t *p_from, *p_to;
	endpoint_t *e_from, *e_to;

	if (up->pid != 1) {
		debug_warn("proc %i is not proc1!\n", up->pid);
		return ERR;
	}

	debug_info("proc %i wants to connect from %i to %i e %i\n",
		up->pid, from, to, to_eid);

	p_from = find_proc(from);
	if (p_from == nil) {
		return ERR;
	}

	p_to = find_proc(to);
	if (p_to == nil) {
		return ERR;
	}

	e_to = proc_find_endpoint(p_to, to_eid);
	if (e_to == nil) {
		return ERR;
	}

	e_from = proc_add_endpoint_connect(p_from, e_to);
	if (e_from == nil) {
		return ERR;
	}

	return e_from->id;
}

	size_t
sys_intr_register(struct intr_mapping *map)
{
	if (up->pid != 1) {
		debug_warn("proc %i is not proc1!\n", up->pid);
		return ERR;
	}

	return irq_add_user(map);
}

	size_t
sys_intr_exit(int irqn)
{
	debug_info("%i called sys intr_exit\n", up->pid);

	return irq_exit();
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
	[SYSCALL_INTR_EXIT]        = (void *) &sys_intr_exit,
	[SYSCALL_PROC_NEW]         = (void *) &sys_proc_new,
	[SYSCALL_PROC_SETUP]       = (void *) &sys_proc_setup,
	[SYSCALL_ENDPOINT_LISTEN]  = (void *) &sys_endpoint_listen,
	[SYSCALL_ENDPOINT_CONNECT] = (void *) &sys_endpoint_connect,
	[SYSCALL_INTR_REGISTER]    = (void *) &sys_intr_register,
	[SYSCALL_DEBUG]            = (void *) &sys_debug,
};

