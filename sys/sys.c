#include "head.h"

cap_t *
proc_find_cap(obj_proc_t *p, int cid)
{
	cap_t *c;

	for (c = p->caps; c != nil; c = c->next) {
		if (c->id == cid) {
			return c;
		}
	}

	return nil;
}

size_t
sys_obj_create(int cid, size_t pa, size_t len)
{
	obj_untyped_t *o;
	cap_t *c;

	debug_info("%i obj create %i 0x%x 0x%x\n", 
		up->pid, cid, pa, len);

	size_t mlen = 0x1000;

	while (true) {
		if (mlen > len) {
			debug_info("%i obj create bad size %i\n", up->pid, len);
			return ERR;
		} else if (mlen == len) {
			break;
		} else {
			mlen <<= 1;
		}
	}

	c = proc_find_cap(up, cid);
	if (c == nil) {
		debug_warn("%i obj create couldnt find cap %i\n", 
			up->pid, cid);
		return ERR;
	} else if (c->perm != 0) {
		debug_warn("%i obj create cap %i bad perm %i\n", 
			up->pid, cid, c->perm);
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
	c->obj = (obj_head_t *) o;

	debug_info("%i obj create 0x%x cap id %i\n", 
		up->pid, o, c->id);

	return OK;
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
obj_untyped_init(obj_proc_t *p, void *o, size_t n)
{
	obj_untyped_t *u = o;

	u->len = n;

	return OK;
}

int
obj_endpoint_init(obj_proc_t *p, void *o, size_t n)
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
obj_caplist_init(obj_proc_t *p, void *o, size_t n)
{
	obj_caplist_t *l = o;
	cap_t *c;
	int i;

	debug_info("proc %i making caplist 0x%x size %i\n", p->pid, o, n);

	l->n = n;

	for (i = 0; i < n; i++) {
		c = &l->caps[i];

		c->id = p->next_cap_id++;
		c->perm = 0;
		c->obj = nil;

		c->next = p->caps->next;
		p->caps->next = c;
	}
	
	return l->caps[0].id;
}

size_t
sys_obj_retype(int cid, int type, size_t n)
{
	obj_untyped_t *o;
	size_t len;
	cap_t *c;

	debug_info("%i obj retype %i type %i len %i\n", 
		up->pid, cid, type, n);

	if (type < 0 || OBJ_type_n < type) {
		return ERR;
	} 

	c = proc_find_cap(up, cid);
	if (c == nil) {
		debug_warn("cap not found\n");
		return ERR;
	}

	debug_info("cap found\n");

	if (c->obj->type != OBJ_untyped) {
		return ERR;
	}
	
	o = (obj_untyped_t *) c->obj;

	debug_info("find size\n");

	len = obj_size_funcs[type](n);
	if (len == 0) {
		return ERR;
	}

	debug_info("need size %i\n", len);

	len = align_up(len, sizeof(size_t));

	if (o->len < len) {
		return ERR;
	}

	o->h.type = type;
	o->h.refs = 1;

	return obj_init_funcs[type](up, o, n);
}

size_t
sys_obj_split(int cid, int nid)
{
	obj_untyped_t *o, *no;
	cap_t *c, *nc;
	size_t len;

	debug_info("%i obj split %i\n", up->pid, cid);

	c = proc_find_cap(up, cid);
	if (c == nil) {
		debug_warn("cap not found\n");
		return ERR;
	} else if (!(c->perm & CAP_write)) {
		return ERR;
	} else if (c->obj->type != OBJ_untyped) {
		return ERR;
	}

	nc = proc_find_cap(up, nid);
	if (c == nil) {
		debug_warn("cap not found\n");
		return ERR;
	} else if (nc->perm != 0) {
		return ERR;
	}

	o = (obj_untyped_t *) c->obj;

	len = align_up(o->len / 2, sizeof(size_t));

	debug_info("%i split obj 0x%x of len %i into %i\n",
		up->pid, o->len, len);

	if (len < sizeof(obj_untyped_t)) {
		return ERR;
	}

	no = (void *) (((uint8_t *) o) + len);

	no->h.type = OBJ_untyped;
	no->h.refs = 1;
	no->len = len;

	o->h.type = OBJ_untyped;
	o->h.refs = 1;
	o->len = len;

	nc->obj = (void *) no;
	nc->perm = CAP_read | CAP_write;

	debug_info("%i add cap %i for obj 0x%x\n", 
		up->pid, nc->id, no);

	return nc->id;
}

size_t
sys_obj_merge(int cid_l, int cid_h)
{
	obj_untyped_t *ol, *oh;
	cap_t *cl, *ch;

	debug_info("%i obj merge %i and %i\n", 
		up->pid, cid_l, cid_h);

	cl = proc_find_cap(up, cid_l);
	ch = proc_find_cap(up, cid_h);

	if (cl == nil || ch == nil) {
		debug_warn("cap not found\n");
		return ERR;
	}

	debug_info("caps found\n");

	if (cl->obj->type != OBJ_untyped) {
		return ERR;
	} else if (ch->obj->type != OBJ_untyped) {
		return ERR;
	}

	if (cl->obj->refs > 1) {
		return ERR;
	} else if (ch->obj->refs > 1) {
		return ERR;
	}
	
	ol = (obj_untyped_t *) cl->obj;
	oh = (obj_untyped_t *) ch->obj;

	if (ol->len != oh->len) {
		return ERR;
	}

	if (((size_t) ol) + ol->len != (size_t) oh) {
		return ERR;
	}

	ol->len += oh->len;

	ch->obj = nil;
	ch->perm = 0;

	return OK;
}

bool
endpoint_get_pending(obj_endpoint_t *e, 
	int *pid, uint8_t *m, 
	cap_t *o)
{
	obj_proc_t *p;

	if (e->signal != 0) {
		((uint32_t *) m)[0] = e->signal;
		e->signal = 0;
		*pid = PID_SIGNAL;
		return true;
	}

	for (p = e->waiting.head; p != nil; p = p->wnext) {
		if (p->state == PROC_block_send) {
			p->state = PROC_block_reply;

			memcpy(m, p->m, MESSAGE_LEN);

			if (p->give != nil && p->give->perm != 0) {
				if (o == nil) {
					panic("%i send to %i with cap but not accepting, TODO: handle this\n", p->pid, up->pid);
				}

				debug_info("%i recv cap %i from %i to cap %i\n", 
					up->pid, p->give->id, p->pid, o->id);

				o->perm = p->give->perm;
				o->obj = p->give->obj;

				p->give->perm = 0;
				p->give->obj = nil;
			}

			*pid = p->pid;
			return true;
		}
	}

	return false;
}

cap_t *
recv(cap_t *from, int *pid, uint8_t *m, cap_t *o)
{
	cap_t *c;

	while (true) {
		if (from != nil) {
			if (endpoint_get_pending((obj_endpoint_t *) from->obj, 
					pid, m, o)) 
			{
				return from;
			}
			
			up->recv_from = (obj_endpoint_t *) from->obj;
		} else {
			for (c = up->caps; c != nil; c = c->next) {
				if ((c->perm & CAP_read) && c->obj->type == OBJ_endpoint) {
					if (endpoint_get_pending((obj_endpoint_t *) c->obj, 
							pid, m, o)) 
					{
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
reply(obj_endpoint_t *e, int pid, uint8_t *m, cap_t *o)
{
	obj_proc_t *p;

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

	if (o != nil) {
		debug_info("%i offer cap id %i\n", up->pid, o->id);
		if (p->give == nil) {
			debug_warn("%i offer cap to %i but they are not accepting\n",
				up->pid, p->pid);
		} else {
			p->give->perm = o->perm;
			p->give->obj = o->obj;
			o->perm = 0;
			o->obj = nil;
		}
	}

	proc_ready(p);
	schedule(p);
	
	return OK;
}

int
mesg(obj_endpoint_t *e, uint8_t *rq, uint8_t *rp, cap_t *o)
{
	obj_proc_t *p;

	p = e->holder;

	memcpy(up->m, rq, MESSAGE_LEN);

	up->give = o;

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

	if (o != nil) {
		if (o->perm == 0) {
			debug_info("%i mesg cap didn't get cap\n",
				up->pid);
		} else {
			debug_info("%i mesg cap got cap %i obj 0x%x\n", 
				up->pid, o->id, o->obj);

			if (o->obj->type == OBJ_endpoint && (o->perm & CAP_read)) {
				((obj_endpoint_t *) o->obj)->holder = up;
			}
		}
	}

	return OK;
}

int
signal(obj_endpoint_t *e, uint32_t s)
{
	obj_proc_t *p;

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
sys_recv(int from, int *pid, uint8_t *m, int cid)
{
	cap_t *c, *o;

	debug_info("%i recv %i\n", up->pid, cid);

	if (from == EID_ANY) {
		c = nil;
	} else {
		c = proc_find_cap(up, from);
		if (c == nil) {
			return ERR;
		} else if (!(c->perm & CAP_read) || c->obj->type != OBJ_endpoint) {
			return ERR;
		}
	}

	if (cid == 0) {
		o = nil;
	} else {
		o = proc_find_cap(up, cid);
		if (o == nil) {
			return ERR;
		} else if (o->perm != 0) {
			return ERR;
		}
	}

	c = recv(c, pid, m, o);
	if (c == nil) {
		return ERR;
	}

	return c->id;
}

size_t
sys_mesg(int to, uint8_t *rq, uint8_t *rp, int cid)
{
	cap_t *c, *o;

	debug_info("%i mesg %i\n", up->pid, to);

	c = proc_find_cap(up, to);
	if (c == nil) {
		debug_warn("%i mesg cap %i not found\n", up->pid, to);
		return ERR;
	} else if (!(c->perm & CAP_write) || c->obj->type != OBJ_endpoint) {
		debug_warn("%i mesg cap %i perm %i or type %i bad\n",
			 up->pid, to, c->perm, c->obj->type);
		return ERR;
	}

	if (cid != 0) {
		o = proc_find_cap(up, cid);
		if (o == nil) {
			debug_warn("%i mesg cap %i not found\n", up->pid, cid);
			return ERR;
		}
	} else {
		o = nil;
	}

	return mesg((obj_endpoint_t *) c->obj, rq, rp, o);
}

size_t
sys_reply(int to, int pid, uint8_t *m, int cid)
{
	cap_t *c, *o;

	debug_info("%i reply %i pid %i, give cap %i\n", up->pid, to, pid, cid);

	c = proc_find_cap(up, to);
	if (c == nil) {
		return ERR;
	} else if (!(c->perm & CAP_read) || c->obj->type != OBJ_endpoint) {
		return ERR;
	}

	if (cid != 0) {
		o = proc_find_cap(up, cid);
		if (o == nil) {
			debug_warn("%i mesg cap %i not found\n", up->pid, cid);
			return ERR;
		} else if (o->perm == 0) {
			return ERR;
		}
	} else {
		o = nil;
	}

	return reply((obj_endpoint_t *) c->obj, pid, m, o);
}

size_t
sys_signal(int to, uint32_t s)
{
	cap_t *c;

	debug_info("%i signal %i 0x%x\n", up->pid, to, s);

	c = proc_find_cap(up, to);
	if (c == nil) {
		return ERR;
	} else if (!(c->perm & CAP_write) || c->obj->type != OBJ_endpoint) {
		return ERR;
	}

	return signal((obj_endpoint_t *) c->obj, s);
}

	size_t
sys_endpoint_connect(int cid, int nid)
{
	obj_endpoint_t *l;
	cap_t *c, *n;

	c = proc_find_cap(up, cid);
	if (c == nil) {
		return ERR;
	} else if (c->obj->type != OBJ_endpoint) {
		return ERR;
	} else {
		l = (obj_endpoint_t *) c->obj;
	}

	n = proc_find_cap(up, nid);
	if (n == nil) {
		return ERR;
	} else if (n->perm != 0) {
		return ERR;
	}

	n->perm = CAP_write;
	n->obj = (void *) l;

	debug_info("%i connect endpoint obj 0x%x, cap id %i\n", 
		up->pid, c->obj, n->id);

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
obj_proc_size(size_t n)
{
	if (n != 1) return 0;
	return sizeof(obj_proc_t);
}

int
obj_proc_init(obj_proc_t *p, void *a, size_t n)
{
	obj_proc_t *o = a;

	memset(((uint8_t *) o) + sizeof(obj_head_t), 0,
		sizeof(obj_proc_t) - sizeof(obj_head_t));

	return OK;
}

size_t
sys_proc_setup(int cid, size_t vspace, size_t priority, int p_eid)
{
	obj_proc_t *o;
	cap_t *c, *pe;

	debug_info("%i proc setup cid %i with vspace 0x%x, priority %i\n", 
		up->pid, cid, vspace, priority);

	c = proc_find_cap(up, cid);
	if (c == nil) {
		debug_warn("%i couln't find cid %i\n", up->pid, cid);
		return ERR;
	} else if (c->obj->type != OBJ_proc) {
		debug_warn("%i obj type bad %i : %i\n", up->pid, cid, c->obj->type);
		return ERR;
	} else {
		o = (obj_proc_t *) c->obj;
	}

	pe = proc_find_cap(up, p_eid);
	if (pe == nil) {
		debug_warn("%i couln't find cid %i\n", up->pid, p_eid);
		return ERR;
	} else if (!(pe->perm & CAP_write)) {
		return ERR;
	} else if (pe->obj->type != OBJ_endpoint) {
		return ERR;
	}

	if (proc_init(o, priority, vspace) != OK) {
		debug_warn("proc_init failed\n");
		return ERR;
	}

	o->initial_caps[1].perm = CAP_write;
	o->initial_caps[1].obj = pe->obj;

	pe->perm = 0;

	debug_info("new proc %i setup with vspace 0x%x, priority %i\n", 
		o->pid, o->vspace, o->priority);

	return o->pid;
}

size_t
sys_proc_start(int cid, size_t pc, size_t sp)
{
	obj_proc_t *o;
	cap_t *c;

	debug_info("%i proc start cid %i\n", up->pid, cid);

	c = proc_find_cap(up, cid);
	if (c == nil) {
		return ERR;
	} else if (c->obj->type != OBJ_proc) {
		return ERR;
	} else {
		o = (obj_proc_t *) c->obj;
	}

	debug_info("%i proc start pid %i\n", up->pid, o->pid);

	func_label(&o->label, 
		(size_t) o->kstack, KSTACK_LEN,
		(size_t) &proc_start,
		pc, sp);

	debug_info("%i proc ready pid %i\n", up->pid, o->pid);

	proc_ready(o);

	return OK;
}

		size_t
sys_debug(char *m)
{
	debug_warn("%i debug %s\n", up->pid, m);

	return OK;
}

