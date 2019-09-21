#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <stdarg.h>
#include <string.h>
#include <dev_reg.h>

#define NO_RSP -2

struct dev {
	int pid;
	char name[DEV_REG_name_len];
};

#define MAX_DEVS 16
struct dev devs[MAX_DEVS] = {0};

struct find_waiting {
	bool in_use;
	int eid, pid;
	char name[DEV_REG_name_len];
};

#define MAX_WAITING 16
struct find_waiting waiting[MAX_WAITING] = {0};

struct find_waiting *
get_free_find_waiting(void)
{
	int i;
	for (i = 0; i < MAX_WAITING; i++) {
		if (!waiting[i].in_use) {
			waiting[i].in_use = true;
			return &waiting[i];
		}
	}

	return nil;
}

void
check_waiting(int id)
{
	union dev_reg_rsp rp;
	int i;

	for (i = 0; i < MAX_WAITING; i++) {
		if (waiting[i].in_use && strcmp(waiting[i].name, devs[id].name)) {

			rp.find.type = DEV_REG_find_rsp;
			rp.find.pid = devs[id].pid;
			rp.find.id = id;
			rp.find.ret = OK;

			reply(waiting[i].eid, waiting[i].pid, &rp);

			waiting[i].in_use = false;
		}
	}
}

static void
handle_find(int eid, int from, 
		union dev_reg_req *rq)
{
	union dev_reg_rsp rp;
	struct find_waiting *w;
	int i;

	rp.find.type = DEV_REG_find_rsp;

	for (i = 0; i < MAX_DEVS && devs[i].pid > 0; i++) {
		if (strncmp(devs[i].name, rq->find.name, sizeof(devs[i].name))) {
			rp.find.pid = devs[i].pid;
			rp.find.id = i;
			rp.find.ret = OK;
			reply(eid, from, &rp);
			return;
		}
	}

	if (rq->find.block) {
		w = get_free_find_waiting();
		if (w != nil) {
			w->pid = from;
			w->eid = eid;
			memcpy(w->name, rq->find.name, DEV_REG_name_len);
			return;
		}
	}

	rp.find.ret = ERR;
	reply(eid, from, &rp);
}

static void
handle_register(int eid, int from, 
		union dev_reg_req *rq)
{
	union dev_reg_rsp rp;
	int i;

	rp.reg.type = DEV_REG_register_rsp;

	for (i = 0; i < MAX_DEVS && devs[i].pid > 0; i++)
		;

	if (i == MAX_DEVS) {
		rp.reg.ret = ERR;
		reply(eid, from, &rp);
		return;
	}

	devs[i].pid = from;
	memcpy(devs[i].name, rq->reg.name, sizeof(devs[i].name));

	rp.reg.id = i;

	check_waiting(i);

	rp.reg.ret = OK;

	reply(eid, from, &rp);
}

static void
handle_list(int eid, int from, 
		union dev_reg_req *rq)
{
	union dev_reg_rsp rp;

	rp.list.type = DEV_REG_list_rsp;
	rp.list.ret = ERR;

	reply(eid, from, &rp);
}

	void
main(void)
{
	union dev_reg_req rq;
	int eid, from;

	while (true) {
		if ((eid = recv(EID_ANY, &from, &rq)) < 0) continue;
		if (from == PID_NONE) continue;

		switch (rq.type) {
			case DEV_REG_find_req:
				handle_find(eid, from, &rq);
				break;

			case DEV_REG_register_req:
				handle_register(eid, from, &rq);
				break;

			case DEV_REG_list_req:
				handle_list(eid, from, &rq);
				break;

			default:
				break;
		}
	}
}

