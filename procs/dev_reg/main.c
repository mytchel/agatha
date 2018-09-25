#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <dev_reg.h>

struct dev {
	int pid;
	char name[DEV_REG_name_len];
};

struct dev devs[16] = {0};

	int
handle_find(int from, 
		union dev_reg_req *rq,
		union dev_reg_rsp *rp)
{
	int i;

	for (i = 0; i < 16 && devs[i].pid > 0; i++) {
		if (strncmp(devs[i].name, rq->find.name, sizeof(devs[i].name))) {
			rp->find.pid = devs[i].pid;
			rp->find.id = i;
			return OK;
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

		send(from, rp_buf);
	}
}

