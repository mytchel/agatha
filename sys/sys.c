#include "head.h"
#include <sysnum.h>

static struct message messages[MAX_MESSAGES];
static size_t n_messages = 0;
static message_t free_messages = nil;

message_t
message_get(void)
{
	message_t m;

	if (free_messages == nil) {
		if (n_messages < LEN(messages)) {
			free_messages = &messages[n_messages++];
			free_messages->next = nil;
		} else {
			return nil;
		}
	}

	m = free_messages;
	free_messages = m->next;

	return m;
}

void
message_free(message_t m)
{
	m->next = free_messages;
	free_messages = m;
}

	int
send(proc_t to, message_t m)
{
	message_t *p;

	m->next = nil;
	for (p = &to->messages; *p != nil; p = &(*p)->next)
		;

	*p = m;

	if (to->state == PROC_recv 
			&& (to->recv_from == -1 || to->recv_from == up->pid)) {
		to->state = PROC_ready;
		schedule(to);
	}

	return OK;
}

int
recv(int from, uint8_t *raw)
{
	message_t *m, n;
	int f;
	
  while (true) {
		for (m = &up->messages; *m != nil; m = &(*m)->next) {
			if (from != -1 && from != (*m)->from)
				continue;

			memcpy(raw, (*m)->body, MESSAGE_LEN);
			f = (*m)->from;

			n = *m;
			*m = (*m)->next;

			message_free(n);

			return f;
		}

		up->recv_from = from;
		up->state = PROC_recv;
		schedule(nil);
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
sys_send(int pid, uint8_t *raw)
{
	message_t m;
	proc_t p;

	debug("%i send to %i\n", up->pid, pid);

	p = find_proc(pid);
	if (p == nil) {
		return ERR;
	}

	m = message_get();

	m->from = up->pid;
	memcpy(m->body, raw, MESSAGE_LEN);

	return send(p, m);
}

size_t
sys_recv(int from, uint8_t *m)
{
	debug("%i receive from %i\n", up->pid, from);
	return recv(from, m);
}

size_t
sys_pid(void)
{
	debug("%i get pid\n", up->pid);

	return up->pid;
}

size_t
sys_proc_new(void)
{
  proc_t p;

	debug("%i proc new\n", up->pid);

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

	size_t
sys_intr_register(int p_id, size_t irqn)
{
	proc_t p;

	debug("%i called sys intr_register with %i, %i\n", up->pid, p_id, irqn);

	if (up->pid != 0) {
		debug("proc %i is not proc0!\n", up->pid);
		return ERR;
	}

	p = find_proc(p_id);
	if (p == nil) {
		debug("didnt find %i\n", p_id);
		return ERR;
	}

	return add_user_irq(irqn, p);
}

void *systab[NSYSCALLS] = {
  [SYSCALL_YIELD]            = (void *) &sys_yield,
  [SYSCALL_SEND]             = (void *) &sys_send,
  [SYSCALL_RECV]             = (void *) &sys_recv,
  [SYSCALL_PID]              = (void *) &sys_pid,
  [SYSCALL_PROC_NEW]         = (void *) &sys_proc_new,
  [SYSCALL_VA_TABLE]         = (void *) &sys_va_table,
  [SYSCALL_INTR_REGISTER]    = (void *) &sys_intr_register,
};
