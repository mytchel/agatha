#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <mesg.h>
#include <stdarg.h>
#include <string.h>
#include <dev_reg.h>
#include <serial.h>
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

static int log_output_pid = -1;
static char *buf;
static size_t buf_size;
static size_t start, end;

void
send_logs(void)
{
	union serial_req rq;

	if (log_output_pid == -1) {
		return;
	} else if (start == end) {
		return;
	}

	rq.write.type = SERIAL_write;

	rq.write.len = end - start;
	/*
	if (start < end) {
		rq.write.len = end - start;
	} else {
		rq.write.len = buf_size - start;
	}
*/
	if (rq.write.len > sizeof(rq.write.data)) {
		rq.write.len = sizeof(rq.write.data);
	}

	memcpy(rq.write.data, buf + start, rq.write.len);

	send(log_output_pid, &rq);

	start += rq.write.len;
	/*
	start = (start + rq.write.len) % buf_size;*/
}

void
add_log(char *s, size_t len)
{
	size_t l;

	memcpy(buf + end, s, len);
	end += len;
	return;

	while (len > 0) {
		if (end < start && end + len >= start) {
			break;
		} else if (end + len > buf_size) {
			l = buf_size - end;
		} else {
			l = len;
		}

		memcpy(buf + end, s, l);

		s += l;
		len -= l;
		end = (end + l) % buf_size;	
	}
}

	void
handle_write_response(union serial_rsp *rp)
{
	send_logs();
}

void
handle_register(int from, union log_req *rq)
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

	send(from, &rp);

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
handle_log(int from, union log_req *rq)
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
		send(from, &rp);
		return;
	}

	if (rq->log.level >= LOG_MAX_LEVEL) {
		rp.log.ret = ERR;
		send(from, &rp);
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
	send(from, &rp);

	send_logs();
}

	void
main(void)
{
	uint8_t m_buf[MESSAGE_LEN];
	union dev_reg_req *rq;
	union dev_reg_rsp *rp;
	uint32_t type;
	size_t pa;
	char s[64];
	size_t len;
	int from, i;

	char *log_output = "serial0";

	rq = (void *) m_buf;
	rq->find.type = DEV_REG_find;
	rq->find.block = true;
	snprintf(rq->find.name, sizeof(rq->find.name),
			"%s", log_output);

	send(DEV_REG_PID, rq);

	start = 0;
	end = 0;

	buf_size = PAGE_ALIGN(0x2000);
	pa = request_memory(buf_size);
	if (pa == nil) {
		exit();
	}

	buf = map_addr(pa, buf_size, MAP_RW|MAP_MEM);
	if (buf == nil) {
		exit();
	}

	len = snprintf(s, sizeof(s),
			"logger using %i bytes mapped as 0x%x->0x%x\n",
			buf_size, pa, buf);
	add_log(s, len);

	for (i = 0; i < MAX_SERVICES; i++) {
		services[i].pid = -1;
	}

	while (true) {
		from = recv(-1, m_buf);
		if (from < 0) continue;

		type = *((uint32_t *) m_buf);

		switch (type) {
			case DEV_REG_find:
				rp = (void *) m_buf;
				if (from == DEV_REG_PID 
						&& rp->find.ret == OK 
						&& log_output_pid == -1) {

					log_output_pid = rp->find.pid;

					send_logs();
				}

				break;

			case SERIAL_write:
				handle_write_response((void *) m_buf);
				break;

			case LOG_register:
				handle_register(from, (void *) m_buf);
				break;

			case LOG_log:
				handle_log(from, (void *) m_buf);
				break;

		}
	}
}

