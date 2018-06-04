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

	if (alive == nil) {
		return nil;
	} else if (up != nil) {
		p = up->next;
	} else {
		p = alive;
	}

	do {
		if (p == nil) {
			p = alive;

		} else if (p->state == PROC_ready) {
			return p;

		} else {
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
		mmu_switch(up->vspace);

		set_systick(10000);

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
	if (pid < MAX_PROCS) {
		return procs[pid].state != PROC_dead ? &procs[pid] : nil;
	} else {
		return nil;
	}
}

