#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <block_dev.h>
#include <dev_reg.h>

int
block_dev_register(struct block_dev *dev)
{
	uint8_t rq_buf[MESSAGE_LEN], rp_buf[MESSAGE_LEN];
	union dev_reg_req *drq = (union dev_reg_req *) rq_buf;
	union dev_reg_rsp *drp = (union dev_reg_rsp *) rp_buf;
	int ret;

	drq->type = DEV_REG_register;
	drq->reg.pid = pid();
	memcpy(drq->reg.name, dev->name, sizeof(drq->reg.name));

	if ((ret = send(DEV_REG_PID, rq_buf)) != OK)
		return ret;

	while (recv(DEV_REG_PID, rp_buf) != DEV_REG_PID)
		;

	if (drp->reg.ret != OK)
		return drp->reg.ret;

	while (true) {

	}

	return OK;
}


