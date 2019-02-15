#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <dev_reg.h>

#define NO_RSP -2

struct dev {
	int pid;
	char name[DEV_REG_name_len];
};

struct dev devs[16] = {0};

struct find_waiting {
	bool in_use;
	int pid;
	char name[DEV_REG_name_len];
};

#define MAX_WAITING 16
struct find_waiting waiting[MAX_WAITING] = {0};

struct find_waiting *
get_find_waiting(void)
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
	uint8_t rp_buf[MESSAGE_LEN];
	union dev_reg_rsp *rp = (union dev_reg_rsp *) rp_buf;
	int i;

	rp->find.type = DEV_REG_find;
	rp->find.pid = devs[id].pid;
	rp->find.id = id;
	rp->find.ret = OK;

	for (i = 0; i < MAX_WAITING; i++) {
		if (waiting[i].in_use && strcmp(waiting[i].name, devs[id].name)) {
			send(waiting->pid, rp_buf);
			waiting[i].in_use = false;
		}
	}
}

	int
handle_find(int from, 
		union dev_reg_req *rq,
		union dev_reg_rsp *rp)
{
	struct find_waiting *w;
	int i;

	for (i = 0; i < 16 && devs[i].pid > 0; i++) {
		if (strncmp(devs[i].name, rq->find.name, sizeof(devs[i].name))) {
			rp->find.pid = devs[i].pid;
			rp->find.id = i;
			return OK;
		}
	}

	if (rq->find.block) {
		w = get_find_waiting();
		if (w != nil) {
			w->pid = from;
			memcpy(w->name, rq->find.name, DEV_REG_name_len);
			return NO_RSP;
		}
	}
		
	return ERR;
}

	int
handle_register(int from, 
		union dev_reg_req *rq,
		union dev_reg_rsp *rp)
{
	int i;

	for (i = 0; i < 16 && devs[i].pid > 0; i++)
		;

	if (i == 16)
		return ERR;

	devs[i].pid = from;
	memcpy(devs[i].name, rq->reg.name, sizeof(devs[i].name));

	rp->reg.id = i;

	check_waiting(i);

	return OK;
}

	int
handle_list(int from, 
		union dev_reg_req *rq,
		union dev_reg_rsp *rp)
{
	return ERR;
}

	void
main(void)
{
	uint8_t rq_buf[MESSAGE_LEN], rp_buf[MESSAGE_LEN];
	union dev_reg_req *rq = (union dev_reg_req *) rq_buf;
	union dev_reg_rsp *rp = (union dev_reg_rsp *) rp_buf;
	int from;

	while (true) {
		if ((from = recv(-1, rq_buf)) < 0)
			continue;

		rp->untyped.type = rq->type;

		switch (rq->type) {
			case DEV_REG_find:
				rp->untyped.ret = handle_find(from, rq, rp);
				break;

			case DEV_REG_register:
				rp->untyped.ret = handle_register(from, rq, rp);
				break;

			case DEV_REG_list:
				rp->untyped.ret = handle_list(from, rq, rp);
				break;

			default:
				rp->untyped.ret = ERR;
				break;
		}

		if (rp->untyped.ret != NO_RSP)
			send(from, rp_buf);
	}
}

