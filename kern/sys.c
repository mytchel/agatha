#include "head.h"
#include <sysnum.h>

int
send_intr(proc_t p, size_t intr)
{
	if (p->intr != 0) {
		return ERR;
	}

	p->intr = intr;
	if (p->state == PROC_recv) {
		p->state = PROC_ready;
		schedule(p);
	}

	return OK;
}

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
		if (up->intr != 0) {
			fill_intr_m(m, up->intr);
			up->intr = 0;

			return OK;

		} else if ((f = up->waiting) != nil) {
			debug("got message from %i\n", f->pid);
			up->waiting = f->wnext;
			f->wnext = nil;

			memcpy(m, f->m, MESSAGE_LEN);

			f->state = PROC_ready;

			return OK;

		} else {
			debug("going to sleep\n");
			up->state = PROC_recv;
			schedule(nil);
			debug("%i has been woken up\n", up->pid);
		}
	}

	return ERR;
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
sys_proc_new(void)
{
  proc_t p;

	debug("%i called sys proc_new\n", up->pid);

	if (up->pid != 0) {
		debug("proc %i is not proc0!\n", up->pid);
		return ERR;
	}

  p = proc_new();
  if (p == nil) {
    return ERR;
  }

  debug("new proc %i\n", p->pid);

  func_label(&p->label, (size_t) p->kstack, KSTACK_LEN,
        (size_t) &proc_start);

	return p->pid;
}

size_t
sys_va_table(int p_id, size_t pa)
{
	proc_t p;

	debug("%i called sys va_table with %i, 0x%h\n", up->pid, p_id, pa);

	if (up->pid != 0) {
		debug("proc %i is not proc0!\n", up->pid);
		return ERR;
	}

	p = find_proc(p_id);
	if (p == nil) {
		debug("didnt find %i\n", p_id);
		return ERR;
	}

	p->vspace = pa;

  return OK;
}

void *systab[NSYSCALLS] = {
  [SYSCALL_YIELD]            = (void *) &sys_yield,
  [SYSCALL_SEND]             = (void *) &sys_send,
  [SYSCALL_RECV]             = (void *) &sys_recv,
  [SYSCALL_PROC_NEW]         = (void *) &sys_proc_new,
  [SYSCALL_VA_TABLE]         = (void *) &sys_va_table,
};

