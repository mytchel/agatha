#include "head.h"

/* TODO: 

	Endpoint pool should be per process and allocated
	by the process.

   */

#define MAX_CAP 64
static cap_t caps[MAX_CAP] = { 0 };
static size_t n_caps = 0;
static cap_t *free_caps = nil;

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

size_t
sys_obj_create(size_t pa, size_t len)
{
	obj_untyped_t *o;
	cap_t *c;

	debug_info("%i obj create 0x%x 0x%x\n", up->pid, pa, len);

	if (up->pid != ROOT_PID) {
		return ERR;
	}

	c = cap_create(up);
	if (c == nil) {
		return ERR;
	}

	o = kernel_map(pa, len, true);
	if (o == nil) {
		return ERR;
	}

	o->h.refs = 0;
	o->h.type = OBJ_untyped;

	obj_untyped_init(up, o, len);

	c->perm = CAP_write | CAP_read;
	c->obj = o;

	cap_add(up, c);

	debug_info("%i obj create 0x%x cap id %i\n", 
		up->pid, o, c->id);

	return c->id;
}

size_t 
obj_untyped_size(size_t n)
{
	if (n < sizeof(obj_untyped_t)) {
		return 0;
	} 

	return n;
}

size_t 
obj_endpoint_size(size_t n)
{
	if (n != 1) return 0;
	return sizeof(obj_endpoint_t);
}

size_t 
obj_caplist_size(size_t n)
{
	return sizeof(obj_caplist_t) 
			+ sizeof(cap_t) * n;
}

int
obj_untyped_init(proc_t *p, void *o, size_t n)
{
	obj_untyped_t *u = o;

	u->len = n - sizeof(obj_untyped_t);
	u->used = 0;

	return OK;
}

int
obj_endpoint_init(proc_t *p, void *o, size_t n)
{
	obj_endpoint_t *e = o;

	debug_info("proc %i making endpoint 0x%x\n", p->pid, e);

	e->holder = p;
	e->signal = 0;
	e->waiting.head = nil;
	e->waiting.tail = nil;
	
	return OK;
}

int
obj_caplist_init(proc_t *p, void *o, size_t n)
{
	debug_info("proc %i making caplist 0x%x size %i\n", p->pid, o, n);
	
	return OK;
}

size_t
sys_obj_retype(int cid, obj_type_t type, size_t n)
{
	obj_untyped_t *o, *no;
	cap_t *c, *nc;
	size_t len;
	int r;

	debug_info("%i obj retype %i type %i len %i\n", 
		up->pid, cid, type, n);

	c = proc_find_cap(up, cid);
	if (c == nil) {
		debug_warn("cap not found\n");
		return ERR;
	}

	debug_info("cap found\n");

	o = c->obj;
	if (o->h.type != OBJ_untyped) {
		return ERR;
	}

	debug_info("find size\n");

	len = obj_size_funcs[type](n);
	if (len == 0) {
		return ERR;
	}

	debug_info("need size %i\n", len);

	/* TODO: does this align up or down */
	debug_warn("align len was %i\n", len);
	len = align_up(len, sizeof(size_t));
	debug_warn("align len now %i\n", len);

	if (o->len - o->used < len) {
		return ERR;
	}

	no = (void *) &o->body[o->used];
	no->h.type = type;
	no->h.refs = 1;

	r = obj_init_funcs[type](up, no, n);
	if (r != OK) {
		return r;
	}

	o->used += len;

	nc = cap_create(up);
	if (nc == nil) {
		return ERR;
	}

	nc->obj = no;
	nc->perm = CAP_read | CAP_write;

	cap_add(up, nc);

	return nc->id;
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
	c->obj = (void *) e;

	debug_info("proc %i connect endpoint 0x%x, cap %i\n",
		p->pid, e, c->id);

	return c;
}

cap_t *
proc_find_cap(proc_t *p, int cid)
{
	cap_t *c;

	if (cid == 0) return nil;

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
obj_proc_size(size_t n)
{
	if (n != 1) return 0;
	return sizeof(obj_proc_t);
}

int
obj_proc_init(proc_t *p, void *a, size_t n)
{
	obj_proc_t *o = a;

	o->proc = nil;

	return OK;
}

size_t
sys_proc_setup(int cid, size_t vspace, size_t priority, int p_eid)
{
	obj_proc_t *o;
	cap_t *c, *pe;
	proc_t *n;

	debug_info("%i proc setup cid %i with vspace 0x%x, priority %i\n", 
		up->pid, cid, vspace, priority);

	c = proc_find_cap(up, cid);
	if (c == nil) {
		debug_warn("%i couln't find cid %i\n", up->pid, cid);
		return ERR;
	} else if (c->obj->h.type != OBJ_proc) {
		debug_warn("%i obj type bad %i : %i\n", up->pid, cid, c->obj->h.type);
		return ERR;
	} else {
		o = (obj_proc_t *) c->obj;
	}

	pe = proc_find_cap(up, p_eid);
	if (pe == nil) {
		debug_warn("%i couln't find cid %i\n", up->pid, p_eid);
		return ERR;
	} else if (pe->obj->h.type != OBJ_endpoint) {
		return ERR;
	}

	n = proc_new(priority, vspace);
	if (n == nil) {
		debug_warn("proc_new failed\n");
		return ERR;
	}

	o->proc = n;
	n->obj = o;

	cap_remove(up, pe);
	cap_add(n, pe);

	debug_info("new proc %i setup with vspace 0x%x, priority %i\n", 
		n->pid, n->vspace, n->priority);

	return OK;
}

size_t
sys_proc_start(int cid, size_t pc, size_t sp)
{
	obj_proc_t *o;
	proc_t *n;
	cap_t *c;

	debug_info("%i proc start cid %i\n", up->pid, cid);

	c = proc_find_cap(up, cid);
	if (c == nil) {
		return ERR;
	} else if (c->obj->h.type != OBJ_proc) {
		return ERR;
	} else {
		o = (obj_proc_t *) c->obj;
	}

	n = o->proc;
	
	debug_info("%i proc start pid %i\n", up->pid, n->pid);

	func_label(&n->label, 
		(size_t) n->kstack, KSTACK_LEN,
		(size_t) &proc_start,
		pc, sp);

	debug_info("%i proc ready pid %i\n", up->pid, n->pid);

	proc_ready(n);

	debug_info("%i proc %i started\n", up->pid, n->pid);

	schedule(n);

	return OK;
}

		size_t
sys_debug(char *m)
{
	debug_warn("%i debug %s\n", up->pid, m);

	return OK;
}

