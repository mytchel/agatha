#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <proc0.h>
#include <stdarg.h>
#include <string.h>
#include <block.h>
#include <log.h>

	int
handle_info(struct block_dev *dev,
		int eid, int from,
		union block_req *rq, int cap)
{
	union block_rsp rp;

	log(LOG_INFO, "handle info for pid %i", from);

	rp.info.type = BLOCK_info;
	rp.info.ret = OK;
	rp.info.block_size = dev->block_size;
	rp.info.nblocks = dev->nblocks;

	log(LOG_INFO, "reply info for pid %i", from);

	return reply_cap(eid, from, &rp, cap);
}

	int
handle_read(struct block_dev *dev,
		int eid, int from,
		union block_req *rq, 
		int cap)
{
	union block_rsp rp;
	size_t start, n, off;
	void *addr;
	int ret;

	size_t pa, len;
	int type;
	
	rp.read.type = BLOCK_read;

	if (frame_info(cap, &type, &pa, &len) != OK) {
		rp.read.ret = ERR;
		return reply_cap(eid, from, &rp, cap);
	}

	if (len < rq->read.off + rq->read.len ||
			rq->read.start % dev->block_size != 0 || 
			rq->read.len % dev->block_size != 0) 
	{
		rp.read.ret = ERR;
		return reply_cap(eid, from, &rp, cap);
	}

	start = rq->read.start / dev->block_size;
	n = rq->read.len / dev->block_size;
	off = rq->read.off;

	if (dev->map_buffers) {
		addr = frame_map_anywhere(cap);
		if (addr == nil) {
			rp.read.ret = ERR;
			return reply_cap(eid, from, &rp, cap);
		}

		ret = dev->read_blocks_mapped(dev, 
				((uint8_t *) addr) + off, 
				start, n);

		unmap_addr(cap, addr);

	} else {
		ret = dev->read_blocks(dev, 
				pa + off, 
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
	size_t start, n, off;
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
	off = rq->write.off;

	if (dev->map_buffers) {
		addr = frame_map_anywhere(cap);
		if (addr == nil) {
			rp.write.ret = ERR;
			return reply_cap(eid, from, &rp, cap);
		}

		ret = dev->write_blocks_mapped(dev, 
				((uint8_t *) addr) + off, 
				start, n);

		unmap_addr(cap, addr);

	} else {
		ret = dev->write_blocks(dev, 
				pa + off, 
				start, n);
	}

	rp.write.ret = ret;

	return reply(eid, from, &rp);
}

	int
block_dev_register(struct block_dev *dev)
{
	union block_req brq;
	int cap, from;

	cap = kcap_alloc();
	if (cap < 0) {
		return cap;
	}

	while (true) {
		if (recv_cap(dev->mount_cap, &from, &brq, cap) != OK)
			continue;
		if (from == PID_SIGNAL)
			continue;

		switch (brq.type) {
			case BLOCK_info:
				handle_info(dev, dev->mount_cap, from, &brq, cap);
				break;

			case BLOCK_read:
				handle_read(dev, dev->mount_cap, from, &brq, cap);
				break;

			case BLOCK_write:
				handle_write(dev, dev->mount_cap, from, &brq, cap);
				break;

			default:
				break;
		}
	}

	return OK;
}


