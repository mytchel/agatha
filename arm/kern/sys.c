#include "head.h"
#include "fns.h"
#include <arm/mmu.h>

size_t
sys_obj_create(int fid, int nid)
{
	obj_untyped_t *o;
	obj_frame_t *f;
	cap_t *fc, *nc;

	debug_info("%i obj create from frame 0x%x into cap 0x%x\n",
		up->pid, fid, nid);

	fc = proc_find_cap(up, fid);
	nc = proc_find_cap(up, nid);
	if (fc == nil || nc == nil) {
		debug_warn("%i obj create couldnt find cap %i or %i\n", 
			up->pid, fid, nid);
		return ERR;
	} else if (fc->obj->type != OBJ_frame) {
		debug_warn("%i obj create %i is not frame\n",
			up->pid, fc);
		return ERR;
	} else if (!(fc->perm & CAP_write) || !(fc->perm & CAP_read)) {
		debug_warn("%i obj create frame cap %i bad perm %i\n",
			up->pid, fid, fc->perm);
		return ERR;
	} else if (nc->perm != 0) {
		debug_warn("%i obj create cap %i bad perm %i\n", 
			up->pid, nid, nc->perm);
		return ERR;
	}

	f = (void *) fc->obj;
	if (f->type != FRAME_MEM) {
		return ERR;
	}

	size_t mlen = 0x1000;

	while (true) {
		if (mlen > f->len) {
			debug_info("%i obj create bad size %i\n", up->pid, f->len);
			return ERR;
		} else if (mlen == f->len) {
			break;
		} else {
			mlen <<= 1;
		}
	}

	o = kernel_map(f->pa, f->len, true);
	if (o == nil) {
		return ERR;
	}

	memset(o, 0, f->len);

	o->h.refs = 0;
	o->h.type = OBJ_untyped;

	obj_untyped_init(up, o, f->len);

	nc->perm = CAP_write | CAP_read;
	nc->obj = (obj_head_t *) o;

	f->type = FRAME_KOBJ;

	fc->perm = 0;
	fc->obj = nil;

	debug_info("%i obj create 0x%x cap id 0x%x\n", 
		up->pid, o, nc->id);

	return OK;
}


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

size_t
sys_frame_setup(int cid, int type, size_t pa, size_t len)
{
	obj_frame_t *f;
	cap_t *c;

	debug_info("%i frame setup %i\n", up->pid, cid);

	if (up->pid != ROOT_PID) {
		return ERR;
	}

	c = proc_find_cap(up, cid);
	if (c == nil) {
		return ERR;
	} else if (c->obj->type != OBJ_frame) {
		return ERR;
	} 

	f = (obj_frame_t *) c->obj;
	if (f->type != FRAME_NONE) {
		return ERR;
	}

	f->type = type;
	f->pa = pa;
	f->len = len;
	f->next = nil;

	return OK;
}

size_t
sys_frame_info(int cid, int *type, size_t *pa, size_t *len)
{
	obj_frame_t *f;
	cap_t *c;

	debug_info("%i frame info %i\n", up->pid, cid);

	c = proc_find_cap(up, cid);
	if (c == nil) {
		return ERR;
	} else if (c->obj->type != OBJ_frame) {
		return ERR;
	} 

	f = (obj_frame_t *) c->obj;

	*type = f->type;
	*pa = f->pa;
	*len = f->len;

	return OK;
}

static cap_t *
proc_find_cap_type(obj_proc_t *p, int cid,
	int perm, int type)
{
	cap_t *c;

	c = proc_find_cap(p, cid);
	if (c == nil) {
		debug_warn("%i cap %i not found\n", p->pid, cid);
		return nil;

	} else if ((c->perm & perm) != perm) {
		debug_warn("%i cap %i bad perm 0x%x != expected 0x%x\n", 
			p->pid, c->id, c->perm, perm);
		return nil;

	} else if (c->perm != 0 && c->obj->type != type) {
		debug_warn("%i cap %i bad type %i != expected %i\n", 
			p->pid, c->id, c->obj->type, type);
		return nil;

	} else {
		return c;
	}
}

	size_t
sys_frame_l1_setup(int cid)
{
	obj_frame_t *f;
	uint32_t *l1;
	cap_t *c;

	debug_info("%i frame l1 setup %i\n", up->pid, cid);

	c = proc_find_cap_type(up, cid, CAP_write|CAP_read, OBJ_frame);
	if (c == nil) {
		return ERR;
	} 

	f = (obj_frame_t *) c->obj;
	if (f->type != FRAME_MEM) {
		debug_warn("%i frame %i bad type\n", up->pid, cid);
		return ERR;
	}

	if (align_down(f->pa, 0x4000) != f->pa) {
		debug_warn("0x%x bad align for l1\n", f->pa);
		return ERR;
	}

	if (f->len != 0x4000) {
		debug_warn("0x%x bad len for l1\n", f->len);
		return ERR;
	}

	l1 = kernel_map(f->pa, 0x4000, true);
	if (l1 == nil) {
		debug_warn("0x%x frame l1 setup map failed\n", up->pid);
		return ERR;
	}

	memcpy(l1, kernel_l1, 0x4000);
	
	kernel_unmap(l1, 0x4000);

	f->type = FRAME_L1;
	f->va = nil;
	f->next = nil;
	
	return OK;
}

	size_t
sys_frame_l2_map(int tid, int cid, size_t va)
{
	obj_frame_t *tf, *ff;
	uint32_t *l1, *l2;
	cap_t *tc, *fc;

	debug_info("%i frame l2 map 0x%x into 0x%x at 0x%x\n", 
		up->pid, cid, tid, va);

	tc = proc_find_cap_type(up, tid, CAP_write|CAP_read, OBJ_frame);
	fc = proc_find_cap_type(up, cid, CAP_write|CAP_read, OBJ_frame);
	if (tc == nil || fc == nil) {
		return ERR;
	} 

	tf = (obj_frame_t *) tc->obj;
	ff = (obj_frame_t *) fc->obj;
	if (tf->type != FRAME_L1) {
		debug_warn("%i tid %i bad l1 frame type\n", up->pid, tc->id);
		return ERR;
	} else if (ff->type != FRAME_MEM) {
		debug_warn("%i fid %i bad l2 frame type\n", up->pid, fc->id);
		return ERR;
	}

	l1 = kernel_map(tf->pa, 0x4000, true);
	if (l1 == nil) {
		return ERR;
	}

	l2 = kernel_map(ff->pa, ff->len, true);
	if (l2 == nil) {
		kernel_unmap(l1, 0x4000);
		return ERR;
	}
	
	mmu_invalidate();

	debug_info("%i frame l2 map 0x%x, 0x%x at 0x%x\n", 
		up->pid, ff->pa, ff->len, va);

	memset(l2, 0, ff->len);

	/* TODO: check that there is nothing mapped before */
	map_l2(l1, ff->pa, va, ff->len);

	kernel_unmap(l2, ff->len);
	kernel_unmap(l1, 0x4000);
	
	mmu_invalidate();

	ff->type = FRAME_L2;
	ff->va = va;

	ff->next = tf->next;
	tf->next = ff;

	fc->perm = 0;
	fc->obj = nil;

	return OK;
}

obj_frame_t *
get_remove_l1_mapping(obj_frame_t *tf, int type, size_t va, size_t len)
{
	obj_frame_t **f;
	for (f = &tf->next; *f != nil; f = &(*f)->next) {
		if ((*f)->type != type) continue;
		if ((*f)->va == va && (*f)->len == len) {
			obj_frame_t *t = *f;
			*f = (*f)->next;
			t->next = nil;
			t->va = nil;
			return t;
		}
	}

	return nil;
}

	size_t
sys_frame_l2_unmap(int tid, int nid, size_t va, size_t len)
{
	debug_warn("todo l2 unmap\n");
	return ERR;
	/*
	obj_frame_t *tf, *ff;
	cap_t *tc, *fc;

	debug_info("%i frame l2 unmap into %i from %i at 0x%x, 0x%x\n", 
		up->pid, nid, tid, va, len);

	tc = proc_find_cap_type(up, tid, CAP_write|CAP_read, OBJ_frame);
	fc = proc_find_cap_type(up, nid, 0, 0);

	if (tc == nil || fc == nil) {
		return ERR;
	}
	
	tf = (obj_frame_t *) tc->obj;
	if (tf->type != FRAME_L1) {
		return ERR;
	}

	debug_warn("todo l2 unmap\n");
	return ERR;
	*/
}

	size_t
sys_frame_map(int tid, int cid, size_t va)
{
	obj_frame_t *tf, *ff;
	uint32_t *l1, *l2;
	cap_t *tc, *fc;
	size_t l2_l1x;

	debug_info("%i frame map 0x%x into 0x%x at 0x%x\n", 
		up->pid, cid, tid, va);

	tc = proc_find_cap_type(up, tid, CAP_write|CAP_read, OBJ_frame);
	fc = proc_find_cap_type(up, cid, CAP_write|CAP_read, OBJ_frame);
	if (tc == nil || fc == nil) {
		return ERR;
	} 

	debug_info("have caps\n");

	tf = (obj_frame_t *) tc->obj;
	ff = (obj_frame_t *) fc->obj;

	if (tf->type != FRAME_L1) {
		debug_warn("%i frame map into %i but not l1\n",
			up->pid, tid);
		return ERR;
	} 
	
	debug_info("l1 ok\n");

	uint32_t tex, c, b, o;
	uint32_t ap = AP_RW_RW;
	uint32_t perm;

	if (ff->type == FRAME_MEM) {
		tex = 7;
		c = 1;
		b = 0;
	} else if (ff->type == FRAME_DEV) {
		tex = 0;
		c = 0;
		b = 1;
	} else {
		debug_warn("%i frame map 0x%x bad frame type %i\n",
			up->pid, cid, ff->type);
		return ERR;
	}

	debug_info("%i map 0x%x (pa=0x%x) 0x%x from cid 0x%x\n",
		up->pid, va, ff->pa, ff->len, cid);

	perm = L2_SMALL |
		tex << 6 |
		ap << 4 |
		c << 3 |
		b << 2;

	debug_info("map l1 0x%x (pa=0x%x) for changing\n",
		tid, tf->pa);

	l1 = kernel_map(tf->pa, 0x4000, true);
	if (l1 == nil) {
		return ERR;
	}

	mmu_invalidate();

	l2_l1x = 0;
	l2 = nil;

	debug_info("map pages\n");

	for (o = 0; o < ff->len; o += PAGE_SIZE) {
		if (l2 == nil || L1X(va + o) != l2_l1x) {
			debug_info("map l2 for 0x%x\n", va+o);

			if (l2 != nil) {
				debug_info("unmap l2 for editing\n");
				kernel_unmap(l2, PAGE_SIZE);
			}

			l2_l1x = L1X(va + o);
			if (l1[l2_l1x] == 0) {
				panic("%i trying to map without l2 at 0x%x\n",
					up->pid, va + o);

				return ERR;
			}

			l2 = kernel_map(L1PA(l1[l2_l1x]), PAGE_SIZE, true);
			if (l2 == nil) {
				panic("map failed\n");
				break;
			}

			mmu_invalidate();

			debug_info("mapped l2 0x%x to 0x%x\n", 
				L1PA(l1[l2_l1x]), l2);
		}

		if (l2[L2X(va + o)] != L2_FAULT) {
			panic("%i trying to map over already mapped 0x%x -> 0x%x\n",
				up->pid, va + o, l2[L2X(va+o)]);

			return ERR;
		}

		debug_info("%i mapped 0x%x to 0x%x\n",
			up->pid, ff->pa + o, va + o);

		l2[L2X(va+o)] = (ff->pa + o) | perm;
	}

	if (l2 != nil) {
		debug_info("unmap l2 for editing\n");
		kernel_unmap(l2, PAGE_SIZE);
	}
	
	debug_info("unmap l1 for editing\n");
	kernel_unmap(l1, 0x4000);
	
	mmu_invalidate();

	ff->va = va;
	ff->next = tf->next;
	tf->next = ff;

	fc->perm = 0;
	fc->obj = nil;

	return OK;
}

		size_t
sys_frame_unmap(int tid, int nid, size_t va, size_t len)
{
	obj_frame_t *tf, *ff;
	uint32_t *l1, *l2;
	size_t l2_l1x, o, pa;
	cap_t *tc, *fc;

	debug_info("%i frame unmap into 0x%x from 0x%x at 0x%x, 0x%x\n", 
		up->pid, nid, tid, va, len);

	tc = proc_find_cap_type(up, tid, CAP_write|CAP_read, OBJ_frame);
	fc = proc_find_cap_type(up, nid, 0, 0);
	if (tc == nil || fc == nil) {
		return ERR;
	}

	debug_info("have caps\n");

	tf = (obj_frame_t *) tc->obj;

	if (tf->type != FRAME_L1) {
		debug_warn("%i frame unmap from %i but not l1\n",
			up->pid, tid);
		return ERR;
	}

	ff = get_remove_l1_mapping(tf, FRAME_MEM, va, len);
	if (ff == nil) {
		ff = get_remove_l1_mapping(tf, FRAME_DEV, va, len);
	}

	if (ff == nil) {
		debug_warn("couldn't find mapping\n");
		return ERR;
	}

	debug_info("%i unmap 0x%x (pa=0x%x) 0x%x into cid 0x%x\n",
		up->pid, va, ff->pa, ff->len, nid);

	debug_info("map l1 %i 0x%x for changing\n",
		tid, tf->pa);

	l1 = kernel_map(tf->pa, 0x4000, true);
	if (l1 == nil) {
		return ERR;
	}
	
	mmu_invalidate();

	l2_l1x = 0;
	l2 = nil;

	debug_info("unmap pages\n");

	pa = nil;

	for (o = 0; o < len; o += PAGE_SIZE) {
		if (l2 == nil || L1X(va + o) != l2_l1x) {
			debug_info("map l2 for 0x%x\n", va+o);

			if (l2 != nil) {
				debug_info("unmap l2 for editing\n");
				kernel_unmap(l2, PAGE_SIZE);
			}

			l2_l1x = L1X(va + o);
			if (l1[l2_l1x] == 0) {
				panic("%i trying to unmap without l2 at 0x%x\n",
					up->pid, va + o);

				return ERR;
			}

			l2 = kernel_map(L1PA(l1[l2_l1x]), PAGE_SIZE, true);
			if (l2 == nil) {
				panic("map failed\n");
				return ERR;
			}
	
			mmu_invalidate();

			debug_info("mapped l2 0x%x to 0x%x\n", 
				L1PA(l1[l2_l1x]), l2);
		}

		if (l2[L2X(va + o)] == 0) {
			panic("%i trying to unmap nothing 0x%x\n",
				up->pid, va + o);

			return ERR;
		}

		if (pa != nil && L2PA(l2[L2X(va+o)]) != pa + o) {
			debug_warn("%i unmap not continuous\n");
			break;

		} else if (pa == nil) {
			pa = L2PA(l2[L2X(va+o)]);
		}

		l2[L2X(va+o)] = 0;

		debug_info("%i unmapped 0x%x to 0x%x\n",
			up->pid, ff->pa + o, va + o);
	}

	if (l2 != nil) {
		debug_info("unmap l2 for editing\n");
		kernel_unmap(l2, PAGE_SIZE);
	}
	
	debug_info("unmap l1 for editing\n");
	kernel_unmap(l1, 0x4000);

	mmu_invalidate();

	debug_info("%i unmapped from 0x%x 0x%x 0%x into cap %i\n",
		up->pid, tid, pa, o, nid);

	fc->obj = (void *) ff;
	fc->perm = CAP_read|CAP_write;

	return OK;
}

size_t
obj_frame_size(size_t n)
{
	return sizeof(obj_frame_t);
}

int
obj_frame_init(obj_proc_t *p, void *o, size_t n)
{
	obj_frame_t *f = o;
	
	f->pa = 0;
	f->len = 0;
	f->type = FRAME_NONE;

	return OK;
}

size_t (*obj_size_funcs[OBJ_type_n])(size_t n) = {
	[OBJ_untyped]             = obj_untyped_size,
	[OBJ_endpoint]            = obj_endpoint_size,
	[OBJ_caplist]             = obj_caplist_size,
	[OBJ_proc]                = obj_proc_size,
	
	[OBJ_intr]                = obj_intr_size,
	[OBJ_frame]               = obj_frame_size,
};

int (*obj_init_funcs[OBJ_type_n])(obj_proc_t *p, void *o, size_t n) = {
	[OBJ_untyped]             = obj_untyped_init,
	[OBJ_endpoint]            = obj_endpoint_init,
	[OBJ_caplist]             = obj_caplist_init,
	[OBJ_proc]                = obj_proc_init,
	
	[OBJ_intr]                = obj_intr_init,
	[OBJ_frame]               = obj_frame_init,
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
	[SYSCALL_PROC_SET_PRIORITY]= (void *) &sys_proc_set_priority,
	[SYSCALL_PROC_START]       = (void *) &sys_proc_start,

	[SYSCALL_INTR_INIT]        = (void *) &sys_intr_init,
	[SYSCALL_INTR_CONNECT]     = (void *) &sys_intr_connect,
	[SYSCALL_INTR_ACK]         = (void *) &sys_intr_ack,
	
	[SYSCALL_FRAME_SETUP]      = (void *) &sys_frame_setup,
	[SYSCALL_FRAME_INFO]       = (void *) &sys_frame_info,
	[SYSCALL_FRAME_L1_SETUP]   = (void *) &sys_frame_l1_setup,
	[SYSCALL_FRAME_L2_MAP]     = (void *) &sys_frame_l2_map,
	[SYSCALL_FRAME_L2_UNMAP]   = (void *) &sys_frame_l2_unmap,
	[SYSCALL_FRAME_MAP]        = (void *) &sys_frame_map,
	[SYSCALL_FRAME_UNMAP]      = (void *) &sys_frame_unmap,
	
	[SYSCALL_DEBUG]            = (void *) &sys_debug,
};

