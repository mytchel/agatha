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
recv(int from, uint8_t *raw)
{
	message_t *m, n;
	int f;
	
	char s[64] = ""; 	
	for (n = up->messages; n != nil; n = n->next)
		snprintf(s + strlen(s), sizeof(s) - strlen(s),
				"%i ", n->from);

	debug(DEBUG_INFO, "%i has messages from %s\n", up->pid, s);	
	
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

	int
send_h(proc_t to, message_t m, uint8_t *recv_buf)
{
	message_t *p;
	int r;

	m->next = nil;
	for (p = &to->messages; *p != nil; p = &(*p)->next)
		;

	*p = m;

	message_t n;
	char s[64] = ""; 	
	for (n = to->messages; n != nil; n = n->next)
		snprintf(s + strlen(s), sizeof(s) - strlen(s),
				"%i ", n->from);

	debug(DEBUG_INFO, "%i now has messages from %s\n", to->pid, s);	
	
	if (to->state == PROC_recv && (to->recv_from == -1 || to->recv_from == up->pid)) {

		if (recv_buf != nil)  {
			up->state = PROC_recv;
			up->recv_from = to->pid;
		}

		to->state = PROC_ready;
		schedule(to); 
	}

	if (recv_buf != nil) {
		r = recv(to->pid, recv_buf);
		if (r == to->pid) {
			return OK;
		} else {
			return r;
		}
	} else {
		return OK;
	}
}

	int
send(proc_t to, message_t m)
{
	return send_h(to, m, nil);
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

	debug(DEBUG_INFO, "%i send to %i\n", up->pid, pid);

	p = find_proc(pid);
	if (p == nil) {
		return ERR;
	}

	m = message_get();
	if (m == nil) {
		debug(DEBUG_WARN, "out of messages\n");
		return ERR;
	}

	m->from = up->pid;
	memcpy(m->body, raw, MESSAGE_LEN);

	return send(p, m);
}

	size_t
sys_recv(int from, uint8_t *m)
{
	int ret;

	debug(DEBUG_INFO, "%i receive from %i\n", up->pid, from);
	ret = recv(from, m);
	debug(DEBUG_INFO, "%i received from %i\n", up->pid, ret);
	return ret;
}

	size_t
sys_mesg(int pid, uint8_t *send, uint8_t *recv)
{
	message_t m;
	proc_t p;

	debug(DEBUG_INFO, "%i mesg to %i\n", up->pid, pid);

	p = find_proc(pid);
	if (p == nil) {
		return ERR;
	}

	m = message_get();

	m->from = up->pid;
	memcpy(m->body, send, MESSAGE_LEN);

	return send_h(p, m, recv);
}

	size_t
sys_pid(void)
{
	debug(DEBUG_INFO, "%i get pid\n", up->pid);

	return up->pid;
}

	size_t
sys_exit(void)
{
	debug(DEBUG_INFO, "%i proc exiting\n", up->pid);
	up->state = PROC_dead;
	schedule(nil);
	panic("schedule returned to exit!\n");
	return 0;
}

	size_t
sys_proc_new(void)
{
	proc_t p;

	debug(DEBUG_INFO, "%i proc new\n", up->pid);

	if (up->pid != 0) {
		debug(DEBUG_WARN, "proc %i is not proc0!\n", up->pid);
		return ERR;
	}

	p = proc_new();
	if (p == nil) {
		debug(DEBUG_INFO, "proc_new failed\n");
		return ERR;
	}

	debug(DEBUG_INFO, "new proc %i\n", p->pid);

	func_label(&p->label, (size_t) p->kstack, KSTACK_LEN,
			(size_t) &proc_start);

	proc_ready(p);

	return p->pid;
}

	size_t
sys_va_table(int p_id, size_t pa)
{
	proc_t p;

	debug(DEBUG_INFO, "%i called sys va_table with %i, 0x%x\n", up->pid, p_id, pa);

	if (up->pid != 0) {
		debug(DEBUG_WARN, "proc %i is not proc0!\n", up->pid);
		return ERR;
	}

	p = find_proc(p_id);
	if (p == nil) {
		debug(DEBUG_INFO, "didnt find %i\n", p_id);
		return ERR;
	}

	p->vspace = pa;

	return OK;
}

	size_t
sys_intr_register(int p_id, size_t irqn)
{
	proc_t p;

	debug(DEBUG_INFO, "%i called sys intr_register with %i, %i\n", up->pid, p_id, irqn);

	if (up->pid != 0) {
		debug(DEBUG_WARN, "proc %i is not proc0!\n", up->pid);
		return ERR;
	}

	p = find_proc(p_id);
	if (p == nil) {
		debug(DEBUG_INFO, "didnt find %i\n", p_id);
		return ERR;
	}

	return add_user_irq(irqn, p);
}

void *systab[NSYSCALLS] = {
	[SYSCALL_YIELD]            = (void *) &sys_yield,
	[SYSCALL_SEND]             = (void *) &sys_send,
	[SYSCALL_RECV]             = (void *) &sys_recv,
	[SYSCALL_MESG]             = (void *) &sys_mesg,
	[SYSCALL_PID]              = (void *) &sys_pid,
	[SYSCALL_EXIT]             = (void *) &sys_exit,
	[SYSCALL_PROC_NEW]         = (void *) &sys_proc_new,
	[SYSCALL_VA_TABLE]         = (void *) &sys_va_table,
	[SYSCALL_INTR_REGISTER]    = (void *) &sys_intr_register,
};

