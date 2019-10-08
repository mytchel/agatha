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
	c->prev = nil;
	c->next = nil;

	debug_info("%i create cap %i\n", p->pid, c->id);

	return c;
}

void
cap_free(proc_t *p, cap_t *c)
{
	debug_info("%i free cap %i\n", p->pid, c->id);

	c->next = free_caps;
	free_caps = c;
}

void
cap_add(proc_t *p, cap_t *c)
{
	debug_info("%i add cap %i\n", p->pid, c->id);

	c->prev = p->caps;
	c->next = p->caps->next;

	if (c->next != nil) {
		c->next->prev = c;
	}
	
	p->caps->next = c;
}

void
cap_remove(proc_t *p, cap_t *c)
{
	debug_info("%i rem cap %i\n", p->pid, c->id);

	c->prev->next = c->next;
	if (c->next != nil)
		c->next->prev = c->prev;
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

	cap_add(p, c);

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

	cap_add(p, c);

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
endpoint_get_pending(obj_endpoint_t *e, int *pid, uint8_t *m, int *cid, cap_t *t)
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
			p->state = PROC_block_reply;

			memcpy(m, p->m, MESSAGE_LEN);

			if (p->give != nil) {
				if (t == nil) {
					panic("%i send to %i with cap but not accepting, TODO: handle this\n", p->pid, up->pid);
				}

				debug_info("%i recv cap %i from %i to cap %i\n", 
					up->pid, p->give->id, p->pid, t->id);

				t->perm = p->give->perm;
				t->obj = p->give->obj;

				cap_remove(p, p->give);
				cap_free(p, p->give);
				p->give = nil;

				cap_add(up, t);
				*cid = t->id;

			} else if (t != nil) {
				cap_free(up, t);
				*cid = -1;
			}

			*pid = p->pid;
			return true;
		}
	}

	return false;
}

cap_t *
recv(cap_t *from, int *pid, uint8_t *m, int *cid)
{
	cap_t *c, *t;

	if (cid != nil) {
		t = cap_create(up);
		if (t == nil) {
			return nil;
		}
	} else {
		t = nil;
	}

	while (true) {
		if (from != nil) {
			if (endpoint_get_pending((obj_endpoint_t *) from->obj, pid, m, cid, t)) {
				return from;
			}
			
			up->recv_from = (obj_endpoint_t *) from->obj;
		} else {
			for (c = up->caps; c != nil; c = c->next) {
				if ((c->perm & CAP_read) && c->obj->h.type == OBJ_endpoint) {
					if (endpoint_get_pending((obj_endpoint_t *) c->obj, pid, m, cid, t)) {
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
	cap_t *c;

	if (cid != 0) {
		c = proc_find_cap(up, cid);
		if (c == nil) {
			return ERR;
		}
	} else {
		c = nil;
	}

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

	if (c != nil) {
		debug_info("%i offer cap id %i\n", up->pid, cid);
		if (p->take == nil) {
			debug_warn("%i offer cap to %i but %i not accepting\n",
				up->pid, p->pid, p->pid);
		} else {
			p->take->perm = c->perm;
			p->take->obj = c->obj;
			cap_remove(up, c);
			cap_free(up, c);
		}
	}

	proc_ready(p);
	schedule(p);
	
	return OK;
}

int
mesg(obj_endpoint_t *e, uint8_t *rq, uint8_t *rp, int *cid)
{
	proc_t *p;
	cap_t *c;

	p = e->holder;

	memcpy(up->m, rq, MESSAGE_LEN);

	if (cid != nil) {
		if (*cid != 0) {
			debug_info("%i give cap id %i\n", up->pid, *cid);
			c = proc_find_cap(up, *cid);
			if (c == nil) {
				return ERR;
			}

			up->give = c;
		} else {
			up->give = nil;
		}

		up->take = cap_create(up);
		if (up->take == nil) {
			return ERR;
		}
	} else {
		up->give = nil;
		up->take = nil;
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
		c = up->take;
		if (c->perm == 0) {
			debug_info("%i mesg cap didn't get cap\n");
			cap_free(up, c);
			*cid = -1;
		} else {
			debug_info("%i mesg cap got cap %i\n", c->id);
			cap_add(up, c);
			*cid = c->id;
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

size_t
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

size_t
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

size_t
sys_reply(int to, int pid, uint8_t *m, int cid)
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

size_t
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
sys_debug(char *m)
{
	debug_warn("%i debug %s\n", up->pid, m);

	return OK;
}

