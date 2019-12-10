#include "head.h"

#define MIN_TIME_SLICE        10

static void add_to_list_tail(proc_list_t *l, obj_proc_t *p);
static void remove_from_list(proc_list_t *l, obj_proc_t *p);

struct ready_list {
	struct proc_list queue[2];
	int q;
};

struct ready_list ready[PRIORITY_MAX] = { 0 };

obj_proc_t *up = nil;

static uint32_t nextpid = 1;
static obj_proc_t *procs[MAX_PROCS] = { nil };

	static void
add_to_list_tail(proc_list_t *l, obj_proc_t *p)
{
	p->slist = l;

	p->snext = nil;
	p->sprev = l->tail;

	if (l->tail == nil) {
		l->head = p;
	} else {
		l->tail->snext = p;
	}

	l->tail = p;
}

	static void
remove_from_list(proc_list_t *l, obj_proc_t *p)
{
	p->slist = nil;

	if (p->sprev != nil) {
		p->sprev->snext = p->snext;
	} else {
		l->head = p->snext;
	}

	if (p->snext != nil) {
		p->snext->sprev = p->sprev;
	} else {
		l->tail = p->sprev;
	}
}

	static obj_proc_t *
next_proc(void)
{
	obj_proc_t *p, *n;
	int q;

#if DEBUG_LEVEL & DEBUG_SCHED_V
	int i;
	for (i = 0; i < MAX_PROCS; i++) {
		p = procs[i];
		if (p == nil) continue;
		if (p->slist == nil) {
			debug_sched_v("proc %i pri %i in state %i\n", 
					p->pid, p->priority, p->state);
		} else {
			int list = p->slist == &ready[p->priority].queue[0] ? 0 : 1;
			debug_sched_v("proc %i pri %i in state %i in list %i\n", 
					p->pid, p->priority, p->state, list);
		}
	}
#endif

	q = PRIORITY_MAX - 1;
	while (q >= 0) {
		debug_sched_v("check priority %i ready queue %i\n", q, ready[q].q);

		p = ready[q].queue[ready[q].q].head; 
		while (p != nil) {
			n = p->snext;

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
		for (p = ready[q].queue[ready[q].q].head; p != nil; p = p->snext) {
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
schedule(obj_proc_t *n)
{
	if (up != nil) {
		size_t passed = systick_passed();

		debug_sched("proc %i ran for %i ticks\n", up->pid, passed);

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

		} else if (up->state == PROC_ready && up->slist == nil) {
			add_to_list_tail(&ready[up->priority]
				.queue[ready[up->priority].q], 
				up);
		}
	}

	if (n != nil && n->state == PROC_ready) {
		if (n->ts > MIN_TIME_SLICE) {
			debug_sched("use given\n");
			up = n;

		} else if (n->slist == nil) {
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

	int
proc_init(obj_proc_t *p)
{
	int pid;

	pid = nextpid++;

	procs[pid] = p;

	memset(&p->label, 0, sizeof(label_t));

	p->pid = pid;
	p->priority = 0;
	
	p->ts = SYSTICK;

	p->slist = nil;
	p->sprev = nil;
	p->snext = nil;

	p->wprev = nil;
	p->wnext = nil;
	p->state = PROC_fault;
	
	return OK;
}

	int
proc_free(obj_proc_t *p)
{
	debug_sched("free %i\n", p->pid);

	if (p->slist != nil) {
		remove_from_list(p->slist, p);
	}

	return OK;
}

	int
proc_fault(obj_proc_t *p)
{
	debug_sched("fault %i\n", p->pid);

	p->state = PROC_fault;
	if (p->slist != nil) {
		remove_from_list(p->slist, p);
	}

	return OK;
}

	int
proc_ready(obj_proc_t *p)
{
	debug_sched("ready %i\n", p->pid);

	p->state = PROC_ready;
	if (p->slist == nil) {
		/* TODO: don't need to do this if we are about
		   to switch to the proc */

		add_to_list_tail(&ready[p->priority]
			.queue[(ready[p->priority].q + 1) % 2], 
			p);
	}

	return OK;
}

int
proc_set_priority(obj_proc_t *p, size_t priority)
{
	if (priority > PRIORITY_MAX) {
		priority = PRIORITY_MAX;
	}

	p->priority = priority;

	if (p->slist != nil) {
		remove_from_list(p->slist, p);

		add_to_list_tail(&ready[p->priority]
			.queue[(ready[p->priority].q + 1) % 2], 
			p);
	}

	return OK;
}

	obj_proc_t *
find_proc(int pid)
{
	if (pid < 0) {
		return nil;
	} else if (pid < MAX_PROCS) {
		return procs[pid];
	} else {
		return nil;
	}
}

