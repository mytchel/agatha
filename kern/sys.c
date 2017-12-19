#include "head.h"
#include <sysnum.h>
	
	int
send(proc_t p, uint8_t *m)
{
	debug("%i ksend to %i\n", up->pid, p->pid);

  if (p->state == PROC_dead) {
		debug("%i is dead\n", p->pid);
		return ERR;
	} else if (p->m_from != -1) {
    debug("proc holding\n");
		return ERR;
	} else {
		p->m_from = up->pid;
		memcpy(p->m, m, MESSAGE_LEN);
		
		if (p->state == PROC_recv) {
			p->state = PROC_ready;
			schedule(p);
		}

		return OK;
	}
}

int
recv(uint8_t *m)
{
	int pid;
	
	debug("%i krecv\n", up->pid);

  while (true) {
		pid = up->m_from;
		if (pid != -1) {
			memcpy(m, up->m, MESSAGE_LEN);
			up->m_from = -1;

			return pid;
			
		} else {
			debug("going to sleep\n");
			up->state = PROC_recv;
			schedule(nil);
			debug("%i has been woken up\n", up->pid);
		}
	}
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
sys_frame_allow(int pid_allowed_to_map)
{
  debug("%i called sys frame_allow with %i\n", up->pid, pid_allowed_to_map);

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
  [SYSCALL_FRAME_ALLOW]      = (void *) &sys_frame_allow,
  [SYSCALL_FRAME_COUNT]      = (void *) &sys_frame_count,
  [SYSCALL_FRAME_INFO_INDEX] = (void *) &sys_frame_info_index,
  [SYSCALL_FRAME_INFO]       = (void *) &sys_frame_info,
};

