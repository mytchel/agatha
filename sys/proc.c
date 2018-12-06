#include "head.h"

#define TIME_SLICE   100000

void add_to_list_head(proc_list_t l, proc_t p);
void add_to_list_tail(proc_list_t l, proc_t p);
void remove_from_list(proc_list_t l, proc_t p);

struct ready_list {
	struct proc_list queue[2];
	int q;
};

struct ready_list ready = { 0 };

proc_t up = nil;

static uint32_t nextpid = 0;
static struct proc procs[MAX_PROCS] = { 0 };

	void
add_to_list_head(proc_list_t l, proc_t p)
{
	p->list = l;

	p->next = l->head;
	p->prev = nil;

	if (l->head == nil) {
		l->tail = p;
	} else {
		l->head->prev = p;
	}

	l->head = p;
}

	void
add_to_list_tail(proc_list_t l, proc_t p)
{
	p->list = l;

	p->next = nil;
	p->prev = l->tail;

	if (l->tail == nil) {
		l->head = p;
	} else {
		l->tail->next = p;
	}

	l->tail = p;
}

	void
remove_from_list(proc_list_t l, proc_t p)
{
	p->list = nil;

	if (p->prev != nil) {
		p->prev->next = p->next;
	} else {
		l->head = p->next;
	}

	if (p->next != nil) {
		p->next->prev = p->prev;
	} else {
		l->tail = p->prev;
	}
}

	static struct proc *
next_proc(void)
{
	proc_t p, n;

	debug("next proc\n");

	p = ready.queue[ready.q].head; 
	
	while (p != nil) {
		n = p->next;

		debug("%i?\n", p->pid);

		if (p->state == PROC_ready) {
			if (p->ts > 0) {
				debug("yes\n");
				return p;

			} else {
				debug("no, put into next ready queue\n");
				remove_from_list(&ready.queue[ready.q], p);
				add_to_list_tail(&ready.queue[(ready.q + 1) % 2], p);
			}

		} else if (p->state == PROC_dead) {
			debug("no, dead\n");
			remove_from_list(&ready.queue[ready.q], p);

		} else if (p->state == PROC_recv) {
			debug("no, blocked\n");
			remove_from_list(&ready.queue[ready.q], p);

		} else {
			debug("proc state unhandled %i\n", p->state);
			remove_from_list(&ready.queue[ready.q], p);
		}

		p = n;
	}

	debug("switch ready queue\n");
	
	ready.q = (ready.q + 1) % 2;
	if (ready.queue[ready.q].head == nil) {
		return nil;
	} 

	for (p = ready.queue[ready.q].head; p != nil; p = p->next) {
		p->ts = TIME_SLICE;
	}

	return next_proc();
}

	void
schedule(proc_t n)
{
	debug("schedule\n");

	if (n != nil) {
		debug("possibly to %i which is in state %i with %i ns\n", n->pid,
				n->state, n->ts);
	}

	if (up != nil) {
		debug("from %i\n", up->pid);

		if (set_label(&up->label)) {
			return;
		}

		up->ts -= systick_passed();
		if (up->ts < 0)
			up->ts = 0;

		debug("up now has %i ns left to run\n", up->ts);

		if (n != nil) {
			debug("give to next %i\n", n->pid);
			n->ts += up->ts;
		}

		if (up->state != PROC_ready) {
			up->ts = 0;

		} else if (up->state == PROC_ready && up->list == nil) {
			debug("out of time and not in a ready queue, put on next ready queue\n");
			add_to_list_tail(&ready.queue[(ready.q + 1) % 2], up);
		}
	}

	if (n != nil && n->ts > 0) {
		debug("switch to given %i\n", n->pid);
		up = n;

	} else {
		debug("find next proc\n");
		up = next_proc();
	}

	if (up != nil) {
		debug("switch to %i at 0x%x for %i\n", up->pid, up->label.pc, up->ts);
		mmu_switch(up->vspace);

		set_systick(up->ts);

		goto_label(&up->label);
	}

	panic("NO PROCS TO RUN!!\n");
}

	proc_t
proc_new(void)
{
	int pid;
	proc_t p;

	pid = nextpid++;

	p = &procs[pid];

	memset(p, 0, sizeof(struct proc));

	p->state = PROC_ready;
	p->pid = pid;

	return p;
}

void
proc_ready(proc_t p)
{
	p->state = PROC_ready;
	add_to_list_tail(&ready.queue[(ready.q + 1) % 2], p);
}

	proc_t
find_proc(int pid)
{
	if (pid < MAX_PROCS) {
		return procs[pid].state != PROC_dead ? &procs[pid] : nil;
	} else {
		return nil;
	}
}

