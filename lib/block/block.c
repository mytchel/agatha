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
		union block_req *rq, int cap)
{
	union block_rsp rp;

	rp.info.type = BLOCK_info;
	rp.info.ret = OK;
	rp.info.block_size = dev->block_size;
	rp.info.nblocks = dev->nblocks;

	return reply_cap(eid, from, &rp, cap);
}

	int
handle_read(struct block_dev *dev,
		int eid, int from,
		union block_req *rq, 
		int cap)
{
	union block_rsp rp;
	size_t start, n;
	void *addr;
	int ret;

	size_t pa, len;
	int type;
	
	rp.read.type = BLOCK_read;

	if (frame_info(cap, &type, &pa, &len) != OK) {
		rp.read.ret = ERR;
		return reply_cap(eid, from, &rp, cap);
	}

	if (len < rq->read.len ||
			rq->read.start % dev->block_size != 0 || 
			rq->read.len % dev->block_size != 0) 
	{
		rp.read.ret = ERR;
		return reply_cap(eid, from, &rp, cap);
	}

	start = rq->read.start / dev->block_size;
	n = rq->read.len / dev->block_size;

	if (dev->map_buffers) {
		addr = frame_map_anywhere(cap);
		if (addr == nil) {
			rp.read.ret = ERR;
			return reply_cap(eid, from, &rp, cap);
		}

		ret = dev->read_blocks_mapped(dev, addr, 
				start, n);

		unmap_addr(addr);

	} else {
		ret = dev->read_blocks(dev, pa, 
				start, n);
	}

	rp.read.ret = ret;

	return reply_cap(eid, from, &rp, cap);
}

	int
handle_write(struct block_dev *dev,
		int eid, int from,
		union block_req *rq, int cap)
{
	union block_rsp rp;
	size_t start, n;
	void *addr;
	int ret;

	size_t pa, len;
	int type;
	
	rp.write.type = BLOCK_write;

	if (frame_info(cap, &type, &pa, &len) != OK) {
		rp.write.ret = ERR;
		return reply_cap(eid, from, &rp, cap);
	}

	if (len < rq->write.len ||
			rq->write.start % dev->block_size != 0 || 
			rq->write.len % dev->block_size != 0) 
	{
		rp.write.ret = ERR;
		return reply_cap(eid, from, &rp, cap);
	}

	start = rq->write.start / dev->block_size;
	n = rq->write.len / dev->block_size;

	if (dev->map_buffers) {
		addr = frame_map_anywhere(cap);
		if (addr == nil) {
			rp.write.ret = ERR;
			return reply_cap(eid, from, &rp, cap);
		}

		ret = dev->write_blocks_mapped(dev, addr, 
				start, n);

		unmap_addr(addr);

	} else {
		ret = dev->write_blocks(dev, pa, 
				start, n);
	}

	rp.write.ret = ret;

	return reply(eid, from, &rp);
}

	int
block_dev_register(struct block_dev *dev)
{
	union block_req brq;
	int eid, from;
	int cap;

	cap = kcap_alloc();
	if (cap < 0) {
		return cap;
	}

	while (true) {
		if ((eid = recv_cap(EID_ANY, &from, &brq, cap)) < 0)
			continue;

		switch (brq.type) {
			case BLOCK_info:
				handle_info(dev, eid, from, &brq, cap);
				break;

			case BLOCK_read:
				handle_read(dev, eid, from, &brq, cap);
				break;

			case BLOCK_write:
				handle_write(dev, eid, from, &brq, cap);
				break;

			default:
				break;
		}
	}

	return OK;
}


