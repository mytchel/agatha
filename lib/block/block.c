#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <proc0.h>
#include <stdarg.h>
#include <string.h>
#include <block.h>
#include <dev_reg.h>

	int
handle_info(struct block_dev *dev,
		int from,
		union block_req *rq)
{
	union block_rsp rp;

	rp.info.type = BLOCK_info_rsp;
	rp.info.ret = OK;
	rp.info.block_size = dev->block_size;
	rp.info.nblocks = dev->nblocks;

	return send(from, &rp);
}

	int
handle_read(struct block_dev *dev,
		int from,
		union block_req *rq)
{
	union block_rsp rp;
	void *addr;
	int ret;

	rp.read.type = BLOCK_read_rsp;

	addr = map_addr(rq->read.pa, rq->read.len, MAP_RW);
	if (addr == nil) {
		rp.read.ret = ERR;
		return send(from, &rp);
	}

	ret = dev->read_blocks(dev, addr, 
			rq->read.start, rq->read.r_len);

	unmap_addr(addr, rq->read.len);
	give_addr(from, rq->read.pa, rq->read.len);

	rp.read.type = BLOCK_read_rsp;
	rp.read.ret = ret;

	return send(from, &rp);
}

	int
handle_write(struct block_dev *dev,
		int from,
		union block_req *rq)
{
	union block_rsp rp;

	rp.write.type = BLOCK_write_rsp;
	rp.write.ret = ERR;

	return send(from, &rp);
}

	int
block_dev_register(struct block_dev *dev)
{
	union dev_reg_req drq;
	union dev_reg_rsp drp;
	union block_req brq;
	int ret, from;

	drq.reg.type = DEV_REG_register_req;
	drq.reg.pid = pid();
	memcpy(drq.reg.name, dev->name, sizeof(drq.reg.name));

	if ((ret = mesg(DEV_REG_PID, &drq, &drp)) != OK)
		return ret;

	if (drp.reg.ret != OK)
		return drp.reg.ret;

	while (true) {
		if ((from = recv(-1, &brq)) < 0)
			continue;

		switch (brq.type) {
			case BLOCK_info_req:
				handle_info(dev, from, &brq);
				break;

			case BLOCK_read_req:
				handle_read(dev, from, &brq);
				break;

			case BLOCK_write_req:
				handle_write(dev, from, &brq);
				break;

			default:
				break;
		}
	}

	return OK;
}


