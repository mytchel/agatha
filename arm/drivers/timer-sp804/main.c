#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <dev_reg.h>
#include <arm/sp804.h>
#include <log.h>
#include <timer.h>

struct timer {
	struct timer *next;

	int pid;
	bool set;
	uint32_t time_ms;
};

static char dev_name[MESSAGE_LEN];
static volatile struct sp804_timer_regs *regs;

static bool is_set = false;
static uint32_t last_set;
static struct timer *timers = nil;

static uint8_t intr_stack[128]__attribute__((__aligned__(4)));
static void intr_handler(int irqn, void *arg)
{
	uint8_t m[MESSAGE_LEN];

	regs->t[0].clr = 1;
	send(pid(), m);

	intr_exit();
}

int
claim_irq(size_t irqn)
{
	union proc0_req rq;
	union proc0_rsp rp;

	rq.irq_reg.type = PROC0_irq_reg_req;
	rq.irq_reg.irqn = irqn;
	rq.irq_reg.func = &intr_handler;
	rq.irq_reg.arg = nil;
	rq.irq_reg.sp = &intr_stack[sizeof(intr_stack)];

	if (mesg(PROC0_PID, &rq, &rp) != OK || rp.irq_reg.ret != OK) {
		return ERR;
	}

	return OK;
}

static void
handle_set(int from, union timer_req *rq)
{
	struct timer *t;

	for (t = timers; t != nil; t = t->next) {
		if (t->pid == from) {
			break;
		}
	}

	if (t == nil) {
		t = malloc(sizeof(struct timer));
		if (t == nil) {
			log(LOG_WARNING, "out of mem");
			return;
		}

		log(LOG_INFO, "adding new timer");

		t->pid = from;

		t->next = timers;
		timers = t;
	}

	t->set = true;
	t->time_ms = rq->set.time_ms;
}

static void
check_timers(void)
{
	struct timer *t;
	uint32_t elipsed = (last_set - regs->t[0].cur) / 1000;

	log(LOG_INFO, "0x%x since last check", elipsed);

	for (t = timers; t != nil; t = t->next) {
		if (!t->set) continue;

		if (t->time_ms <= elipsed) {
			log(LOG_INFO, "timer passed");
			t->set = false;
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
			if (!is_set || t->time_ms < last_set) {
				set = t->time_ms;
				is_set = true;
			}
		}
	}

	if (is_set) {
		log(LOG_INFO, "set timer to 0x%x", set);
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
	int from;

	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];

	size_t regs_pa, regs_len;
	size_t irqn;
	union dev_reg_req drq;
	union dev_reg_rsp drp;

	recv(0, init_m);

	regs_pa = init_m[0];
	regs_len = init_m[1];
	irqn = init_m[2];

	recv(0, dev_name);

	log_init(dev_name);

	regs = map_addr(regs_pa, regs_len, MAP_DEV|MAP_RW);
	if (regs == nil) {
		log(LOG_FATAL, "failed to map registers!");
		return ERR;
	}

	log(LOG_INFO, "on pid %i mapped 0x%x -> 0x%x with irq %i", 
		pid(), regs_pa, regs, irqn);

	timers = nil;

	set_timer();

	if (claim_irq(irqn) != OK) {
		log(LOG_FATAL, "failed to register int");
		return ERR;
	}

	union timer_req rq;
	rq.set.time_ms = 5 * 1000;
	handle_set(3213, &rq);
	
	set_timer();

	drq.type = DEV_REG_register_req;
	drq.reg.pid = pid();
	snprintf(drq.reg.name, sizeof(drq.reg.name),
			"%s", dev_name);

	if (mesg(DEV_REG_PID, &drq, &drp) != OK || drp.reg.ret != OK) {
		log(LOG_WARNING, "failed to register with dev reg!");
		exit(ERR);
	}

	while (true) {
		union timer_req rq;

		from = recv(-1, &rq);

		if (from == pid()) {
			check_timers();
			set_timer();

		} else {
			switch (rq.type) {
			case TIMER_set:
				handle_set(from, &rq);
				break;

			default:
				break;
			}
		}
	}

	return ERR;
}

