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

#if DEBUG_LEVEL & DEBUG_INFO_V	
	message_t pn;
	char s[64] = ""; 	
	for (pn = up->messages; pn != nil; pn = pn->next)
		snprintf(s + strlen(s), sizeof(s) - strlen(s),
				"%i ", pn->from);

	debug_info("%i has messages from %s\n", up->pid, s);	
#endif

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

	if (up->in_irq && recv_buf != nil) {
		debug_warn("%i trying to recv in irq\n");
		return ERR;
	}

	m->next = nil;
	for (p = &to->messages; *p != nil; p = &(*p)->next)
		;

	*p = m;

#if DEBUG_LEVEL & DEBUG_INFO_V	
	message_t n;
	char s[64] = ""; 	
	for (n = to->messages; n != nil; n = n->next)
		snprintf(s + strlen(s), sizeof(s) - strlen(s),
				"%i ", n->from);

	debug_info("%i now has messages from %s\n", to->pid, s);	
#endif

	if (to->state == PROC_recv && 
			(to->recv_from == -1 || to->recv_from == up->pid)) {

		if (recv_buf != nil)  {
			up->state = PROC_recv;
			up->recv_from = to->pid;
		}

		proc_ready(to);
		if (!up->in_irq) {
			schedule(to);
		}
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

	debug_info("%i send to %i\n", up->pid, pid);

	p = find_proc(pid);
	if (p == nil) {
		return ERR;
	}

	m = message_get();
	if (m == nil) {
		debug_warn("out of messages\n");
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

	debug_info("%i receive from %i\n", up->pid, from);
	ret = recv(from, m);
	debug_info("%i received from %i\n", up->pid, ret);
	return ret;
}

	size_t
sys_mesg(int pid, uint8_t *send, uint8_t *recv)
{
	message_t m;
	proc_t p;

	debug_info("%i mesg to %i\n", up->pid, pid);

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
	debug_info("%i get pid\n", up->pid);

	return up->pid;
}

	size_t
sys_exit(size_t code)
{
	debug_info("%i proc exiting with 0x%x\n", up->pid, code);
	up->state = PROC_dead;
	schedule(nil);
	panic("schedule returned to exit!\n");
	return 0;
}

	size_t
sys_proc_new(size_t vspace)
{
	proc_t p;

	debug_info("%i proc new\n", up->pid);

	if (up->pid != 0) {
		debug_warn("proc %i is not proc0!\n", up->pid);
		return ERR;
	}

	p = proc_new();
	if (p == nil) {
		debug_warn("proc_new failed\n");
		return ERR;
	}

	debug_info("new proc %i with vspace 0x%x\n", p->pid, vspace);

	func_label(&p->label, (size_t) p->kstack, KSTACK_LEN,
			(size_t) &proc_start);

	p->vspace = vspace;

	proc_ready(p);

	return p->pid;
}

	size_t
sys_intr_register(struct intr_mapping *map)
{
	if (up->pid != 0) {
		debug_warn("proc %i is not proc0!\n", up->pid);
		return ERR;
	}

	return irq_add_user(map);
}

	size_t
sys_intr_exit(int irqn)
{
	debug_info("%i called sys intr_exit\n", up->pid);

	return irq_exit();
}

size_t
sys_debug(char *m)
{
	debug_info("%i debug %s\n", up->pid, m);

	return OK;
}

void *systab[NSYSCALLS] = {
	[SYSCALL_YIELD]            = (void *) &sys_yield,
	[SYSCALL_SEND]             = (void *) &sys_send,
	[SYSCALL_RECV]             = (void *) &sys_recv,
	[SYSCALL_MESG]             = (void *) &sys_mesg,
	[SYSCALL_PID]              = (void *) &sys_pid,
	[SYSCALL_EXIT]             = (void *) &sys_exit,
	[SYSCALL_INTR_EXIT]        = (void *) &sys_intr_exit,
	[SYSCALL_PROC_NEW]         = (void *) &sys_proc_new,
	[SYSCALL_INTR_REGISTER]    = (void *) &sys_intr_register,
	[SYSCALL_DEBUG]            = (void *) &sys_debug,
};

