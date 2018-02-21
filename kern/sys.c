#include "head.h"
#include <sysnum.h>
	
	int
send(proc_t p, uint8_t *m)
{
	proc_t *n;

	debug("%i ksend to %i\n", up->pid, p->pid);

	memcpy(up->m, m, MESSAGE_LEN);

	up->waiting_on = p;
	up->wnext = nil;

	debug("add to wait list\n");

	for (n = &p->waiting; *n != nil; n = &(*n)->wnext)
		;

	*n = up;

	debug("sleep\n");
	up->state = PROC_send;
	if (p->state == PROC_recv) {
		debug("wake %i\n", p->pid);
		p->state = PROC_ready;
		schedule(p);
	} else {
		schedule(nil);
	}

  up->waiting_on = nil;	

	return OK;
}

int
recv(uint8_t *m)
{
	proc_t f;
	
	debug("%i krecv\n", up->pid);

  while (true) {
		f = up->waiting;
		if (f != nil) {
			debug("got message from %i\n", f->pid);
			up->waiting = f->wnext;
			f->wnext = nil;

			memcpy(m, f->m, MESSAGE_LEN);

			f->state = PROC_ready;
			return f->pid;

		} else {
			debug("going to sleep\n");
			up->state = PROC_recv;
			schedule(nil);
			debug("%i has been woken up\n", up->pid);
		}
	}
}

  size_t
sys_yield(void)
{
  schedule(nil);

  return OK;
}

size_t
sys_send(int pid, uint8_t *m)
{
	proc_t p;

	debug("%i send to 0x%h\n", up->pid, pid);
	
	p = find_proc(pid);
	if (p == nil) {
		debug("didnt find %i\n", pid);
		return ERR;
	}

	return send(p, m);
}

size_t
sys_recv(uint8_t *m)
{
	debug("%i recv to 0x%h\n", up->pid, m);
	
	return recv(m);
}

size_t
sys_proc_new(int f_id)
{
  kframe_t f;
  proc_t p;

	debug("%i called sys proc_new with %i\n", up->pid, f_id);

  f = frame_find_fid(up, f_id);
  if (f == nil) {
    debug("didint find frame %i\n", f_id);
    return ERR;
  }

  p = proc_new();
  if (p == nil) {
    return ERR;
  }

  debug("new proc %i\n", p->pid);

  func_label(&p->label, (size_t) p->kstack, KSTACK_LEN,
        (size_t) &proc_start);

  if (frame_give(up, p, f) != OK) {
    debug("failed to swap vspace\n");
    return ERR;
  }

  p->vspace = f; 

	return p->pid;
}

size_t
sys_frame_create(size_t start, size_t len, int type)
{
  kframe_t f;

	debug("%i called sys frame_create with 0x%h, 0x%h, %i\n", up->pid, start, len, type);

  f = frame_new(start, PAGE_ALIGN(len), type);
  if (f == nil) {
    return ERR;
  }

  frame_add(up, f);

  return f->u.f_id;
}

  size_t
sys_frame_split(int f_id, size_t offset)
{
  kframe_t f, n;

  debug("%i called sys frame_split with %i, 0x%h\n", up->pid, f_id, offset);

  f = frame_find_fid(up, f_id);
  if (f == nil) {
    return ERR;
  } 

  n = frame_split(f, offset);
  if (n == nil) {
    return ERR;
  }

  frame_add(up, n);

  return n->u.f_id;
}

  size_t
sys_frame_merge(int f1, int f2)
{
  debug("%i called sys frame_merge with %i, %i\n", up->pid, f1, f2);

  return ERR;
}

  size_t
sys_frame_give(int pid, int f_id)
{
  kframe_t f;
  proc_t to;

  debug("%i called sys frame_give with %i, %i\n", up->pid, f_id, pid);

  f = frame_find_fid(up, f_id);
  if (f == nil) {
    debug("didint find frame %i\n", f_id);
    return ERR;
  }

  to = find_proc(pid);
  if (to == nil) {
    debug("fained to find proc %i\n", pid);
    return ERR;
  }

  debug("giving frame\n");
  return frame_give(up, to, f);
}

  size_t
sys_frame_map(int t_id, int f_id, 
    void *va, int flags)
{
  kframe_t t, f;

  debug("%i called sys frame_map with %i, %i, 0x%h, %i\n", 
      up->pid, t_id, f_id, va, flags);

  t = frame_find_fid(up, t_id);  
  f = frame_find_fid(up, f_id);

  if (t == nil || f == nil) {
    return ERR;
  }

  return frame_map(t, f, (size_t) va, flags);
}

  size_t
sys_frame_table(int f_id, int flags)
{
  kframe_t f;

  debug("%i called sys frame_table with %i, %i\n", 
      up->pid, f_id, flags);

  f = frame_find_fid(up, f_id);

  if (f == nil) {
    return ERR;
  }

  return frame_table(f, flags);
}


  size_t
sys_frame_unmap(int pid, int f_id)
{
  debug("%i called sys frame_unmap with %i, %i\n", up->pid, pid, f_id);

  return ERR;
}

  size_t
sys_frame_count(void)
{
  debug("%i called sys frame_count, getting %i\n", up->pid, up->frame_count);

  return up->frame_count;
}

  size_t
sys_frame_info_index(struct frame *f, int ind)
{
  kframe_t k;

  debug("%i called sys frame_info_index with %i, 0x%h\n", up->pid, ind, f);

  k = frame_find_ind(up, ind);
  if (k == nil) {
    return ERR;
  }

  memcpy(f, &k->u, sizeof(*f));

  return OK;
}

  size_t
sys_frame_info(struct frame *f, int f_id)
{
  kframe_t k;

  debug("%i called sys frame_info with %i, 0x%h\n", up->pid, f_id, f);

  k = frame_find_fid(up, f_id);
  if (k == nil) {
    return ERR;
  }

  memcpy(f, &k->u, sizeof(*f));

  return OK;
}

void *systab[NSYSCALLS] = {
  [SYSCALL_YIELD]            = (void *) &sys_yield,
  [SYSCALL_SEND]             = (void *) &sys_send,
  [SYSCALL_RECV]             = (void *) &sys_recv,
  [SYSCALL_PROC_NEW]         = (void *) &sys_proc_new,
  [SYSCALL_FRAME_CREATE]     = (void *) &sys_frame_create,
  [SYSCALL_FRAME_SPLIT]      = (void *) &sys_frame_split,
  [SYSCALL_FRAME_MERGE]      = (void *) &sys_frame_merge,
  [SYSCALL_FRAME_GIVE]       = (void *) &sys_frame_give,
  [SYSCALL_FRAME_MAP]        = (void *) &sys_frame_map,
  [SYSCALL_FRAME_TABLE]      = (void *) &sys_frame_table,
  [SYSCALL_FRAME_UNMAP]      = (void *) &sys_frame_unmap,
  [SYSCALL_FRAME_COUNT]      = (void *) &sys_frame_count,
  [SYSCALL_FRAME_INFO_INDEX] = (void *) &sys_frame_info_index,
  [SYSCALL_FRAME_INFO]       = (void *) &sys_frame_info,
};

