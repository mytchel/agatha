#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <proc0.h>
#include <stdarg.h>
#include <string.h>
#include <block.h>

	int
handle_info(struct block_dev *dev,
		int eid, int from,
		union block_req *rq)
{
	union block_rsp rp;

	rp.info.type = BLOCK_info;
	rp.info.ret = OK;
	rp.info.block_size = dev->block_size;
	rp.info.nblocks = dev->nblocks;

	return reply(eid, from, &rp);
}

	int
handle_read(struct block_dev *dev,
		int eid, int from,
		union block_req *rq)
{
	union block_rsp rp;
	size_t start, n;
	void *addr;
	int ret;

	rp.read.type = BLOCK_read;

	if (rq->read.len < rq->read.r_len ||
			rq->read.start % dev->block_size != 0 || 
			rq->read.r_len % dev->block_size != 0) 
	{
		rp.read.ret = ERR;
		return reply(eid, from, &rp);
	}

	start = rq->read.start / dev->block_size;
	n = rq->read.r_len / dev->block_size;

	if (dev->map_buffers) {
		addr = map_addr(rq->read.pa, rq->read.len, MAP_RW);
		if (addr == nil) {
			rp.read.ret = ERR;
			return reply(eid, from, &rp);
		}

		ret = dev->read_blocks_mapped(dev, addr, 
				start, n);

		unmap_addr(addr, rq->read.len);

	} else {
		ret = dev->read_blocks(dev, rq->read.pa, 
				start, n);
	}

	give_addr(from, rq->read.pa, rq->read.len);

	rp.read.ret = ret;

	return reply(eid, from, &rp);
}

	int
handle_write(struct block_dev *dev,
		int eid, int from,
		union block_req *rq)
{
	union block_rsp rp;
	size_t start, n;
	void *addr;
	int ret;

	rp.write.type = BLOCK_write;

	if (rq->write.len < rq->write.w_len ||
			rq->write.start % dev->block_size != 0 || 
			rq->write.w_len % dev->block_size != 0) 
	{
		rp.write.ret = ERR;
		return reply(eid, from, &rp);
	}

	start = rq->write.start / dev->block_size;
	n = rq->write.w_len / dev->block_size;

	if (dev->map_buffers) {
		addr = map_addr(rq->write.pa, rq->write.len, MAP_RW);
		if (addr == nil) {
			rp.write.ret = ERR;
			return reply(eid, from, &rp);
		}

		ret = dev->write_blocks_mapped(dev, addr, 
				start, n);

		unmap_addr(addr, rq->write.len);

	} else {
		ret = dev->write_blocks(dev, rq->write.pa, 
				start, n);
	}

	give_addr(from, rq->write.pa, rq->write.len);

	rp.write.ret = ret;

	return reply(eid, from, &rp);
}

	int
block_dev_register(struct block_dev *dev)
{
	union block_req brq;
	int eid, from;

	while (true) {
		if ((eid = recv(EID_ANY, &from, &brq)) < 0)
			continue;

		switch (brq.type) {
			case BLOCK_info:
				handle_info(dev, eid, from, &brq);
				break;

			case BLOCK_read:
				handle_read(dev, eid, from, &brq);
				break;

			case BLOCK_write:
				handle_write(dev, eid, from, &brq);
				break;

			default:
				break;
		}
	}

	return OK;
}


