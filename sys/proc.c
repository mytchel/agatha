#include "head.h"

#define MIN_TIME_SLICE        10

void add_to_list_tail(proc_list_t *l, proc_t *p);
void remove_from_list(proc_list_t *l, proc_t *p);

struct ready_list {
	struct proc_list queue[2];
	int q;
};

struct ready_list ready[PRIORITY_MAX] = { 0 };

proc_t *up = nil;

static uint32_t nextpid = 1;
static proc_t procs[MAX_PROCS] = { 0 };

	void
add_to_list_tail(proc_list_t *l, proc_t *p)
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
remove_from_list(proc_list_t *l, proc_t *p)
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

	static proc_t *
next_proc(void)
{
	proc_t *p, *n;
	int q;

#if DEBUG_LEVEL >= DEBUG_SCHED_V
	int i;
	for (i = 0; i < MAX_PROCS; i++) {
		if (procs[i].state == PROC_free) continue;
		if (procs[i].list == nil) {
			debug_sched_v("proc %i pri %i in state %i\n", 
					procs[i].pid, procs[i].priority, procs[i].state);
		} else {
			int list = procs[i].list == &ready[procs[i].priority].queue[0] ? 0 : 1;
			debug_sched_v("proc %i pri %i in state %i in list %i\n", 
					procs[i].pid, procs[i].priority, procs[i].state, list);
		}
	}
#endif

	q = PRIORITY_MAX - 1;
	while (q >= 0) {
		debug_sched_v("check priority %i ready queue %i\n", q, ready[q].q);

		p = ready[q].queue[ready[q].q].head; 
		while (p != nil) {
			n = p->next;

			if (p->state == PROC_ready) {
				if (p->ts > MIN_TIME_SLICE) {
					debug_sched_v("%i pri %i is ready\n", p->pid, p->priority);
					return p;

				} else {
					debug_sched_v("put %i into other queue\n", p->pid);
					remove_from_list(&ready[q].queue[ready[q].q], p);
					add_to_list_tail(&ready[q].queue[(ready[q].q + 1) % 2], p);
				}

			} else {
				debug_sched_v("remove %i from ready %i\n", p->pid, q);
				remove_from_list(&ready[q].queue[ready[q].q], p);
			}

			p = n;
		}

		debug_sched_v("switch queues\n");

		ready[q].q = (ready[q].q + 1) % 2;
		for (p = ready[q].queue[ready[q].q].head; p != nil; p = p->next) {
			p->ts = SYSTICK;
		}

		if (ready[q].queue[ready[q].q].head == nil) {
			debug_sched_v("check next priority\n");
			q--;
		} 
	}

	return nil;
}

	void
schedule(proc_t *n)
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
				add_to_list_tail(&ready[up->priority]
					.queue[ready[up->priority].q], 
					up);
			}
		}
	}

	if (n != nil && n->state == PROC_ready) {
		if (n->ts > MIN_TIME_SLICE) {
			debug_sched("use given\n");
			up = n;

		} else if (n->list == nil) {
			debug_sched("put given on next queue\n");
			add_to_list_tail(&ready[n->priority].queue[(ready[n->priority].q + 1) % 2], n);
			up = next_proc();

		} else {
			debug_sched("doing nothing with given\n");
			up = next_proc();
		}
	} else {
		up = next_proc();
	}

	if (up != nil) {
		debug_sched("switch to %i for %i\n", 
				up->pid, up->ts);

		mmu_switch(up->vspace);
		set_systick(up->ts);
		goto_label(&up->label);

	} else {
		panic("no procs to run!\n");
	}
}

	proc_t *
proc_new(int priority, size_t vspace)
{
	int pid;
	proc_t *p;

	pid = nextpid++;

	p = &procs[pid];

	memset(p, 0, sizeof(proc_t));

	p->pid = pid;
	p->vspace = vspace;
	p->priority = priority;
	
	p->ts = SYSTICK;

	return p;
}

	int
proc_free(proc_t *p)
{
	debug_sched("free %i\n", p->pid);

	if (p->list != nil) {
		remove_from_list(p->list, p);
	}

	p->pid = -1;

	return OK;
}

	int
proc_fault(proc_t *p)
{
	debug_sched("fault %i\n", p->pid);

	p->state = PROC_fault;
	if (p->list != nil) {
		remove_from_list(p->list, p);
	}

	return OK;
}

	int
proc_ready(proc_t *p)
{
	debug_sched("ready %i\n", p->pid);

	p->state = PROC_ready;
	if (p->list == nil) {
		add_to_list_tail(&ready[p->priority]
			.queue[(ready[p->priority].q + 1) % 2], 
			p);
	}

	return OK;
}

	proc_t *
find_proc(int pid)
{
	if (pid < MAX_PROCS) {
		return procs[pid].state != PROC_free ? &procs[pid] : nil;
	} else {
		return nil;
	}
}

