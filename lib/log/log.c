#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <stdarg.h>
#include <string.h>
#include <log.h>

static bool setup = false;

int
log_init(char *name)
{
	union log_req rq;
	union log_rsp rp;

	rq.reg.type = LOG_register;
	rq.reg.respond = true;
	snprintf(rq.reg.name, sizeof(rq.reg.name),
		 	"%s", name);

	if (mesg(LOG_PID, &rq, &rp) != OK || rp.reg.ret != OK) {
		return rp.reg.ret;
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

	if (mesg(LOG_PID, &rq, &rp) != OK || rp.log.ret != OK) {
		exit();
	}
}

