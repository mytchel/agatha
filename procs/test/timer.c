#include <types.h>
#include <err.h>
#include <mach.h>
#include <sys.h>
#include <sysobj.h>
#include <c.h>
#include <mesg.h>
#include <proc0.h>
#include <mmu.h>
#include <stdarg.h>
#include <string.h>
#include <block.h>
#include <mbr.h>
#include <fs.h>
#include <fat.h>
#include <timer.h>
#include <log.h>

void
test_timer(void)
{
	union proc0_req prq;
	union proc0_rsp prp;

	prq.get_resource.type = PROC0_get_resource;
	prq.get_resource.resource_type = RESOURCE_type_timer;
	
	int timer_eid = kcap_alloc();
	if (timer_eid < 0) {
		exit(ERR);
	}

	mesg_cap(CID_PARENT, &prq, &prp, timer_eid);
	if (prp.get_resource.ret != OK) {
		exit(ERR);
	}

	int timer_lid = kobj_alloc(OBJ_endpoint, 1);
	if (timer_lid < 0) {
		exit(ERR);
	}
		
	int timer_leid = kcap_alloc();
	if (timer_leid < 0) {
		exit(ERR);
	}

	if (endpoint_connect(timer_lid, timer_leid) != OK) {
		exit(ERR);
	}

	union timer_req rq;
	union timer_rsp rp;

	rq.create.type = TIMER_create;
	rq.create.signal = 0x111;

	mesg_cap(timer_eid, &rq, &rp, timer_leid);
	if (rp.create.ret != OK) {
		exit(ERR);
	}

	int timer_id = rp.create.id;

	int i;
	for (i = 0; i < 4; i++) {
		rq.set.type = TIMER_set;
		rq.set.id = timer_id;
		rq.set.time_ms = 5 * 1000;

		mesg(timer_eid, &rq, &rp);
		if (rp.set.ret != OK) {
			exit(ERR);
		}

		uint8_t t[MESSAGE_LEN];
		int t_pid;

		while (true) {
			recv(timer_lid, &t_pid, t);
			if (t_pid == PID_SIGNAL) {
				uint32_t signal = ((uint32_t *) t)[0];
				log(LOG_INFO, "got signal 0x%x", signal);
				break;
			} else {
				log(LOG_WARNING, "how did we get another message?");
			}
		}
	}

	log(LOG_INFO, "timer test finished");
}

