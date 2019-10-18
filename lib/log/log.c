#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <log.h>

static bool setup = false;
static int log_eid;

int
log_init(char *name)
{
	union proc0_req prq;
	union proc0_rsp prp;

	union log_req lrq;
	union log_rsp lrp;

	prq.get_resource.type = PROC0_get_resource;
	prq.get_resource.resource_type = RESOURCE_type_log;

	log_eid = get_free_cap_id();

	mesg_cap(CID_PARENT, &prq, &prp, log_eid);
	if (prp.get_resource.ret != OK) {
		exit(ERR);
	}

	lrq.reg.type = LOG_register;
	snprintf(lrq.reg.name, sizeof(lrq.reg.name),
		 	"%s", name);

	if (mesg(log_eid, &lrq, &lrp) != OK || lrp.reg.ret != OK) {
		return lrp.reg.ret;
	}

	setup = true;
	return OK;
}

void
log(int level, char *fmt, ...)
{
	union log_req rq;
	union log_rsp rp;
	va_list a;

	if (!setup) return;

	va_start(a, fmt);
	vsnprintf(rq.log.mesg, sizeof(rq.log.mesg),
			fmt, a);
	va_end(a);

	rq.log.type = LOG_log;
	rq.log.level = level;

	mesg(log_eid, &rq, &rp);
}

