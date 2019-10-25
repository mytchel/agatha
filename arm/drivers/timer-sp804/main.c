#include <types.h>
#include <err.h>
#include <mach.h>
#include <sys.h>
#include <sysobj.h>
#include <c.h>
#include <mesg.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <arm/sp804.h>
#include <log.h>
#include <timer.h>

struct timer {
	struct timer *next;
	int id;

	int pid;
	int endpoint;
	uint32_t signal;

	bool set;
	uint32_t time_ms;
};

static volatile struct sp804_timer_regs *regs;

static bool is_set = false;
static uint32_t last_set;
static struct timer *timers = nil;
static int next_id = 0;

static void
handle_create(int eid, int from, union timer_req *rq, int cid)
{
	union timer_rsp rp;
	struct timer *t;
	int n_eid;

	n_eid = cid;

	if (n_eid == 0) {
		log(LOG_WARNING, "cap accept failed");
		return;
	}

	t = malloc(sizeof(struct timer));
	if (t == nil) {
		log(LOG_WARNING, "out of mem");
		return;
	}

	log(LOG_INFO, "create new timer");

	t->id = next_id++;

	t->pid = from;
	t->endpoint = n_eid;
	t->signal = rq->create.signal;

	t->next = timers;
	timers = t;

	rp.create.type = TIMER_create;
	rp.create.ret = OK;
	rp.create.id = t->id;
	reply(eid, from, &rp);
}

static void
handle_set(int eid, int from, union timer_req *rq)
{
	struct timer *t;

	for (t = timers; t != nil; t = t->next) {
		if (t->pid == from && t->id == rq->set.id) {
			break;
		}
	}

	if (t == nil) {
		return;
	}

	t->set = true;
	t->time_ms = rq->set.time_ms;

	union timer_rsp rp;
	rp.set.type = TIMER_set;
	rp.set.ret = OK;
	reply(eid, from, &rp);
}

static void
check_timers(void)
{
	uint32_t elipsed = (last_set - regs->t[0].cur) / 1000;
	struct timer *t;

	log(LOG_INFO, "0x%x since last check", elipsed);

	for (t = timers; t != nil; t = t->next) {
		if (!t->set) continue;

		if (t->time_ms <= elipsed) {
			log(LOG_INFO, "timer passed");
			t->set = false;

			signal(t->endpoint, t->signal);
		} else {
			log(LOG_INFO, "decrease time");
			t->time_ms -= elipsed;
		}
	}
}

static void
set_timer(void)
{
	struct timer *t;
	uint32_t set;
	
	is_set = false;

	for (t = timers; t != nil; t = t->next) {
		if (t->set) {
			if (!is_set || t->time_ms < set) {
				set = t->time_ms;
				is_set = true;
			}
		}
	}

	if (is_set) {
		log(LOG_INFO, "set timer to %i ms", set);
		last_set = set * 1000;
		regs->t[0].ctrl = 0;
		regs->t[0].load	= last_set;
		regs->t[0].ctrl = 
			SP804_CTRL_ENABLE |
			SP804_CTRL_INT_ENABLE |
			SP804_CTRL_SIZE |
			SP804_CTRL_ONE_SHOT;
	} else {
		log(LOG_INFO, "disable timer");
		regs->t[0].ctrl = 0;
	}
}

	int
main(void)
{
	size_t regs_pa, regs_len;
	size_t irqn;

	union proc0_req prq;
	union proc0_rsp prp;
	int eid, from, cid;

	log_init("timer0");
	log(LOG_INFO, "start");

	prq.get_resource.type = PROC0_get_resource;
	prq.get_resource.resource_type = RESOURCE_type_mount;

	int mount_eid;
	mount_eid = kcap_alloc();
	if (mount_eid < 0) {
		exit(ERR);
	}

	mesg_cap(CID_PARENT, &prq, &prp, mount_eid);

	if (prp.get_resource.ret != OK) {
		exit(ERR);
	}

	prq.get_resource.type = PROC0_get_resource;
	prq.get_resource.resource_type = RESOURCE_type_regs;

	log(LOG_INFO, "get regs");
	mesg(CID_PARENT, &prq, &prp);

	if (prp.get_resource.ret != OK) {
		exit(ERR);
	}

	regs_pa  = prp.get_resource.result.regs.pa;
	regs_len = prp.get_resource.result.regs.len;

	regs = map_addr(regs_pa, regs_len, MAP_DEV|MAP_RW);
	if (regs == nil) {
		log(LOG_FATAL, "failed to map registers!");
		return ERR;
	}

	prq.get_resource.type = PROC0_get_resource;
	prq.get_resource.resource_type = RESOURCE_type_int;

	int irq_cap_id;
	irq_cap_id = kcap_alloc();
	if (irq_cap_id < 0) {
		exit(ERR);
	}

	log(LOG_INFO, "get int");
	mesg_cap(CID_PARENT, &prq, &prp, irq_cap_id);

	if (prp.get_resource.ret != OK) {
		exit(ERR);
	}

	irqn = prp.get_resource.result.irqn;

	log(LOG_INFO, "on pid %i mapped 0x%x -> 0x%x with irq %i", 
		pid(), regs_pa, regs, irqn);

	timers = nil;

	log(LOG_INFO, "clear");
	set_timer();

	log(LOG_INFO, "register int");
	int int_eid = kobj_alloc(OBJ_endpoint, 1);
	if (intr_connect(irq_cap_id, int_eid, 0x14) != OK) {
		log(LOG_FATAL, "failed to register int");
		return ERR;
	}

	while (true) {
		union timer_req rq;

		cid = kcap_alloc();

		if ((eid = recv_cap(EID_ANY, &from, &rq, cid)) < 0) {
			return ERR;
		}

		if (from == PID_SIGNAL) {
			kcap_free(cid);

			if (eid == int_eid) {
				uint32_t status_raw = regs->t[0].status_raw;

				regs->t[0].clr = status_raw;

				intr_ack(irq_cap_id);

				log(LOG_INFO, "got int 0x%x, status = 0x%x!", 
					rq.type, status_raw);

				check_timers();
				set_timer();
			} else {
				log(LOG_INFO, "someone is sending us a signal?");
			}
		} else {
			switch (rq.type) {
			case TIMER_create:
				handle_create(eid, from, &rq, cid);
				break;

			case TIMER_set:
				kcap_free(cid);
				handle_set(eid, from, &rq);
				break;

			default:
				kcap_free(cid);
				break;
			}
	
			set_timer();
		}
	}

	return ERR;
}

