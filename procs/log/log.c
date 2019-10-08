#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <mesg.h>
#include <stdarg.h>
#include <string.h>
#include <serial.h>
#include <proc0.h>
#include <log.h>

struct service {
	int pid;
	char name[LOG_SERVICE_NAME_LEN];
};

#define MAX_SERVICES    32
static struct service services[MAX_SERVICES];

char *levels[] = {
	[LOG_FATAL]     = "FATAL",
	[LOG_WARNING]   = "WARNING",
	[LOG_INFO]      = "INFO",
};

static int log_output_eid = -1;
static char buf[2048];
static size_t start = 0, end = 0;

void
send_logs(void)
{
	union serial_req rq;
	union serial_rsp rp;

	if (log_output_eid == -1) {
		exit(ERR);
	}

	while (start != end) {
		rq.write.type = SERIAL_write;

		if (start < end) {
			rq.write.len = end - start;
		} else {
			rq.write.len = sizeof(buf) - start;
		}

		if (rq.write.len > sizeof(rq.write.data)) {
			rq.write.len = sizeof(rq.write.data);
		}

		memcpy(rq.write.data, buf + start, rq.write.len);

		if (mesg(log_output_eid, &rq, &rp) != OK) {
			exit(ERR);
		} else if (rp.write.ret != OK) {
			exit(ERR);
		}

		start = (start + rq.write.len) % sizeof(buf);
	}
}

void
add_log(char *s, size_t len)
{
	size_t l;

	while (len > 0) {
		if (end < start && end + len >= start) {
			break;
		} else if (end + len > sizeof(buf)) {
			l = sizeof(buf) - end;
		} else {
			l = len;
		}

		memcpy(buf + end, s, l);

		s += l;
		len -= l;
		end = (end + l) % sizeof(buf);	
	}
}

void
handle_register(int eid, int from, union log_req *rq)
{
	union log_rsp rp;
	char line[256];
	size_t len;
	int i;

	rp.reg.type = LOG_register;
	rp.reg.ret = ERR;
	
	for (i = 0; i < MAX_SERVICES; i++) {
		if (services[i].pid == -1) {
			services[i].pid = from;
			snprintf(services[i].name, sizeof(services[i].name),
					"%s", rq->reg.name);
			rp.reg.ret = OK;
			break;
		}
	}

	if (rp.reg.ret == OK) {
		len = snprintf(line, sizeof(line),
				"REGISTER %i as %s (slot %i)\n",
				from, rq->reg.name, i);
		add_log(line, len);
	} else {
		len = snprintf(line, sizeof(line),
				"WARNING : failed to register %i as %s\n",
				from, rq->reg.name);
		add_log(line, len);

		for (i = 0; i < MAX_SERVICES; i++) {
			len = snprintf(line, sizeof(line),
					"slot %i is pid %i %s\n", i, services[i].pid, services[i].name);
			add_log(line, len);
		}
	}

	reply(eid, from, &rp);

	send_logs();
}

	struct service *
find_service(int pid)
{
	int i;

	for (i = 0; i < MAX_SERVICES; i++) {
		if (services[i].pid == pid) {
			return &services[i];
		}
	}

	return nil;
}

	void
handle_log(int eid, int from, union log_req *rq)
{
	char line[256];
	union log_rsp rp;
	struct service *s;
	char *level;
	size_t len;

	rp.log.type = LOG_log;

	s = find_service(from);
	if (s == nil) {
		rp.log.ret = ERR;
		reply(eid, from, &rp);
		return;
	}

	if (rq->log.level >= LOG_MAX_LEVEL) {
		rp.log.ret = ERR;
		reply(eid, from, &rp);
		return;
	}

	level = levels[rq->log.level];

	len = snprintf(line, sizeof(line),
			"%s : %s (%i): %s\n",
			level, 
			s->name, s->pid, 
			rq->log.mesg);

	add_log(line, len);

	rp.log.ret = OK;

	reply(eid, from, &rp);

	send_logs();
}

	void
main(int p_eid)
{
	union proc0_req prq;
	union proc0_rsp prp;
	union log_req rq;
	int eid, from, i;

	parent_eid = p_eid;

	for (i = 0; i < MAX_SERVICES; i++) {
		services[i].pid = -1;
	}

	prq.get_resource.type = PROC0_get_resource;
	prq.get_resource.resource_type = RESOURCE_type_serial;

	log_output_eid = 0;
	mesg_cap(parent_eid, &prq, &prp, &log_output_eid);
	if (prp.get_resource.ret != OK) {
		exit(ERR);
	}

	char *s = "log starting\n";
	add_log(s, strlen(s));
	send_logs();

	while (true) {
		if ((eid = recv(EID_ANY, &from, &rq)) < 0) continue;
		if (from == PID_NONE) continue;

		switch (rq.type) {
		case LOG_register:
			handle_register(eid, from, &rq);
			break;

		case LOG_log:
			handle_log(eid, from, &rq);
			break;
		}
	}
}

