#include "head.h"

void add_to_list(struct proc **, struct proc *);
bool remove_from_list(struct proc **, struct proc *);

static uint32_t nextpid = 0;

static struct proc procs[MAX_PROCS] = { 0 };
static proc_t alive = nil;
proc_t up = nil;

	static struct proc *
next_proc(void)
{
	proc_t p;

	debug("find next proc in list: ");

	for (p = alive; p != nil; p = p->next)
		debug("%i[%i] ", p->pid, p->state);
	debug("\n");	

	if (alive == nil) {
		debug("none\n");
		return nil;
	} else if (up != nil) {
		debug("one after %i\n", up->pid);
		p = up->next;
	} else {
		debug("start with alive %i\n", alive->pid);
		p = alive;
	}

	do {
		if (p == nil) {
			debug("loop\n");
			p = alive;

		} else if (p->state == PROC_ready) {
			debug("found %i\n", p->pid);
			return p;

		} else {
			debug("skip blocked %i %i\n", p->pid, p->state);
			if (p->state == PROC_send)
				debug("waiting on %i\n", p->waiting_on->pid);
			else if (p->state == PROC_recv)
				debug("waiting for message\n");
			else
				debug("waiting for what?\n");	

			p = p->next;
		}
	} while (p != up);

	return (p != nil && p->state == PROC_ready) ? p : nil;
}

	void
schedule(proc_t n)
{
	/* Until a better scheduler is created */
	n = nil;

	if (up != nil) {
		if (up->state == PROC_oncpu) {
			up->state = PROC_ready;
		}

		if (set_label(&up->label)) {
			return;
		}
	}

	if (n != nil) {
		up = n;
	} else {
		up = next_proc();
	}


	if (up != nil) {
		debug("SWITCH to %i\n", up->pid);

		mmu_switch(up->vspace);

		set_systick(1000);

		up->state = PROC_oncpu;
		goto_label(&up->label);
	}

	panic("NO PROCS TO RUN!!\n");
}

	void
add_to_list(proc_t *l, proc_t p)
{
	p->next = *l;
	*l = p;
}

	bool
remove_from_list(proc_t *l, proc_t p)
{
	proc_t pt;

	if (*l == p) {
		*l = p->next;
	} else {
		for (pt = *l; pt != nil && pt->next != p; pt = pt->next)
			;

		pt->next = p->next;
	}

	return true;
}

	proc_t
proc_new(void)
{
	int pid;
	proc_t p;

	pid = nextpid++;

	p = &procs[pid];

	memset(p, 0, sizeof(struct proc));

	p->pid = pid;

	p->state = PROC_ready;

	add_to_list(&alive, p);

	return p;
}

	proc_t
find_proc(int pid)
{
	return procs[pid].state != PROC_dead ? &procs[pid] : nil;
}

