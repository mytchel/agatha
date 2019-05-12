#include "head.h"

#define MIN_TIME_SLICE        10

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

#if DEBUG_LEVEL >= DEBUG_SCHED_V
	debug_sched("check ready queue %i\n", ready.q);

	int i;
	for (i = 0; i < MAX_PROCS; i++) {
		if (procs[i].state == PROC_dead) continue;
		if (procs[i].list == nil) {
			debug_sched_v("proc %i in state %i\n", 
					procs[i].pid, procs[i].state);
		} else {
			int list = procs[i].list == &ready.queue[0] ? 0 : 1;
			debug_sched_v("proc %i in state %i in list %i\n", 
					procs[i].pid, procs[i].state, list);
		}
	}
#endif

	p = ready.queue[ready.q].head; 
	while (p != nil) {
		n = p->next;

		if (p->state == PROC_ready) {
			if (p->ts > MIN_TIME_SLICE) {
				debug_sched_v("%i is ready\n", p->pid);
				return p;

			} else {
				debug_sched_v("put %i into other queue\n", p->pid);
				remove_from_list(&ready.queue[ready.q], p);
				add_to_list_tail(&ready.queue[(ready.q + 1) % 2], p);
			}

		} else {
			debug_sched_v("remove %i from ready\n", p->pid);
			remove_from_list(&ready.queue[ready.q], p);
		}

		p = n;
	}

	debug_sched("switch queues\n");

	ready.q = (ready.q + 1) % 2;
	if (ready.queue[ready.q].head == nil) {
		return nil;
	} 

	for (p = ready.queue[ready.q].head; p != nil; p = p->next) {
		p->ts = SYSTICK;
	}

	return next_proc();
}

	void
schedule(proc_t n)
{
	if (up != nil) {
		int passed = systick_passed();

		debug_sched("proc %i ran for %i ticks\n", up->pid, passed);

		if (up->in_irq) {
			debug_sched("proc %i still handling irq\n", up->pid);
			if (set_label(up->irq_label)) {
				return;
			}
		} else {
			if (set_label(&up->label)) {
				debug_sched("proc %i back up\n", up->pid);
				return;
			}

			up->ts -= passed;
			if (up->ts < 0) up->ts = 0;

			if (n != nil) {
				n->ts += up->ts; 
			}

			if (up->state != PROC_ready) {
				up->ts = 0;

			} else if (up->state == PROC_ready && up->list == nil) {
				add_to_list_tail(&ready.queue[ready.q], up);
			}
		}
	}

	irq_run_active();

	if (n != nil && n->state == PROC_ready) {
		if (n->ts > MIN_TIME_SLICE) {
			debug_sched("use given\n");
			up = n;

		} else if (n->list == nil) {
			debug_sched("put given on next queue\n");
			add_to_list_tail(&ready.queue[(ready.q + 1) % 2], n);
			up = next_proc();

		} else {
			debug_sched("doing nothing with given\n");
			up = next_proc();
		}
	} else {
		up = next_proc();
	}

	if (up != nil) {
		debug_sched("switch to %i at 0x%x for %i\n", up->pid, up->label.pc, up->ts);
	
		mmu_switch(up->vspace);
		set_systick(up->ts);
		goto_label(&up->label);

	} else {
		debug_sched("no procs to run\n");

		go_idle();
	}
}

	proc_t
proc_new(size_t vspace, int supervisor)
{
	int pid;
	proc_t p;

	pid = nextpid++;

	p = &procs[pid];

	p->pid = pid;
	p->vspace = vspace;
	p->supervisor = supervisor;

	return p;
}

	int
proc_free(proc_t p)
{
	debug_sched("free %i\n", p->pid);

	if (p->list != nil) {
		remove_from_list(p->list, p);
	}

	memset(p, 0, sizeof(struct proc));

	return OK;
}

	int
proc_fault(proc_t p)
{
	debug_sched("fault %i\n", p->pid);

	p->state = PROC_fault;
	if (p->list != nil) {
		remove_from_list(p->list, p);
	}

	return OK;
}

	int
proc_ready(proc_t p)
{
	debug_sched("ready %i\n", p->pid);

	p->state = PROC_ready;
	if (p->list == nil) {
		add_to_list_tail(&ready.queue[(ready.q + 1) % 2], p);
	}

	return OK;
}

	proc_t
find_proc(int pid)
{
	if (pid < MAX_PROCS) {
		return procs[pid].state != PROC_free ? &procs[pid] : nil;
	} else {
		return nil;
	}
}

