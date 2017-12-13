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

  debug("send debug 0x%h, 0x%h\n", pid, m);

  panic("stop\n");
	
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
  proc_t p;

	debug("%i called sys proc_new with %i\n", up->pid, f_id);

  p = proc_new();
  if (p == nil) {
    return ERR;
  }

  func_label(&p->label, (size_t) p->kstack, KSTACK_LEN,
        (size_t) &proc_start);

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
	debug("%i called sys frame_split with %i, 0x%h\n", up->pid, f_id, offset);
	
	return ERR;
}

size_t
sys_frame_merge(int f1, int f2)
{
	debug("%i called sys frame_merge with %i, %i\n", up->pid, f1, f2);
	
	return ERR;
}

size_t
sys_frame_give(int f_id, int pid)
{
	debug("%i called sys frame_give with %i, %i\n", up->pid, f_id, pid);
	
	return ERR;
}

size_t
sys_frame_map(int t_id, int f_id, 
    void *va, int flags)
{
	debug("%i called sys frame_map with %i, %i, 0x%h, %i\n", 
		up->pid, t_id, f_id, va, flags);
	
	return ERR;
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
	[SYSCALL_FRAME_UNMAP]      = (void *) &sys_frame_unmap,
	[SYSCALL_FRAME_ALLOW]      = (void *) &sys_frame_allow,
	[SYSCALL_FRAME_COUNT]      = (void *) &sys_frame_count,
	[SYSCALL_FRAME_INFO_INDEX] = (void *) &sys_frame_info_index,
	[SYSCALL_FRAME_INFO]       = (void *) &sys_frame_info,
};
	
