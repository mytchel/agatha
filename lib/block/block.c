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
handle_info(struct block_dev *dev,
		int from,
		union block_dev_req *rq,
		union block_dev_rsp *rp)
{
	rp->info.block_size = dev->block_size;
	rp->info.nblocks = dev->nblocks;

	return OK;
}

	int
handle_read(struct block_dev *dev,
		int from,
		union block_dev_req *rq,
		union block_dev_rsp *rp)
{
	void *addr;
	int ret;

	addr = map_addr(rq->read.pa, rq->read.len, MAP_RW);
	if (addr == nil) {
		return ERR;
	}

	ret = dev->read_blocks(dev, addr, 
			rq->read.start, rq->read.r_len);

	unmap_addr(addr, rq->read.len);
	give_addr(from, rq->read.pa, rq->read.len);

	return ret;
}

	int
handle_write(struct block_dev *dev,
		int from,
		union block_dev_req *rq,
		union block_dev_rsp *rp)
{
	return ERR;
}

	int
block_dev_register(struct block_dev *dev)
{
	uint8_t rq_buf[MESSAGE_LEN], rp_buf[MESSAGE_LEN];
	union dev_reg_req *drq = (union dev_reg_req *) rq_buf;
	union dev_reg_rsp *drp = (union dev_reg_rsp *) rp_buf;
	union block_dev_req *brq = (union block_dev_req *) rq_buf;
	union block_dev_rsp *brp = (union block_dev_rsp *) rp_buf;
	int ret, from;

	drq->type = DEV_REG_register;
	drq->reg.pid = pid();
	memcpy(drq->reg.name, dev->name, sizeof(drq->reg.name));

	if ((ret = mesg(DEV_REG_PID, rq_buf, rp_buf)) != OK)
		return ret;

	if (drp->reg.ret != OK)
		return drp->reg.ret;

	while (true) {
		if ((from = recv(-1, rq_buf)) < 0)
			continue;

		brp->untyped.type = brq->type;

		switch (brq->type) {
			case BLOCK_DEV_info:
				brp->untyped.ret = handle_info(dev, from, brq, brp);
				break;

			case BLOCK_DEV_read:
				brp->untyped.ret = handle_read(dev, from, brq, brp);
				break;

			case BLOCK_DEV_write:
				brp->untyped.ret = handle_write(dev, from, brq, brp);
				break;

			default:
				brp->untyped.ret = ERR;
				break;
		}

		send(from, rp_buf);
	}

	return OK;
}


