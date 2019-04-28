#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <arm/mmu.h>
#include <proc0.h>
#include <log.h>

	size_t
request_memory(size_t len)
{
	union proc0_req rq;
	union proc0_rsp rp;

	log(LOG_INFO, "request 0x%x bytes", len);

	if (len == 0) {
		return nil;
	}

	len = PAGE_ALIGN(len);

	rq.addr_req.type = PROC0_addr_req;
	rq.addr_req.pa = nil;
	rq.addr_req.len = len;

	if (mesg(PROC0_PID, &rq, &rp) != OK) {
		return nil;
	}

	log(LOG_INFO, "request 0x%x bytes got ret %i", len, rp.addr_req.ret);
	if (rp.addr_req.ret != OK) {
		return nil;
	}

	return rp.addr_req.pa;
}

	int
release_addr(size_t pa, size_t len)
{
	return give_addr(PROC0_PID, pa, len);
}

int
give_addr(int to, size_t pa, size_t len)
{
	union proc0_req rq;
	union proc0_rsp rp;

	log(LOG_INFO, "give %i 0x%x . 0x%x", to, pa, len);

	rq.addr_give.type = PROC0_addr_give;
	rq.addr_give.to = to;
	rq.addr_give.pa = pa;
	rq.addr_give.len = len;

	if (mesg(PROC0_PID, &rq, &rp) != OK) {
		return ERR;
	}

	return rp.addr_give.ret;
}

int
addr_unmap(size_t va, size_t len)
{
	union proc0_req rq;
	union proc0_rsp rp;

	log(LOG_INFO, "unmap 0x%x . 0x%x", va, len);

	if (PAGE_ALIGN(va) != va) {
		return nil;
	}
	
	len = PAGE_ALIGN(len);

	rq.addr_map.type = PROC0_addr_map;
	rq.addr_map.len = len;
	rq.addr_map.va = va;
	rq.addr_map.pa = 0;
	rq.addr_map.flags = MAP_REMOVE_LEAF;

	if (mesg(PROC0_PID, &rq, &rp) != OK) {
		return ERR;
	}

	log(LOG_INFO, "unmap got %i", rp.addr_map.ret);

	return rp.addr_map.ret;
}

int
addr_map(size_t pa, size_t va, size_t len, int flags)
{
	union proc0_req rq;
	union proc0_rsp rp;
	int r;

	rq.addr_map.type = PROC0_addr_map;
	rq.addr_map.pa = pa;
	rq.addr_map.len = len;
	rq.addr_map.va = va;
	rq.addr_map.flags = flags;

	if ((r = mesg(PROC0_PID, &rq, &rp)) != OK) {
		return r;
	}

	return rp.addr_req.ret;
}

int
addr_map_l2s(size_t pa, size_t va, size_t len)
{
	int r;

	log(LOG_INFO, "map l2 0x%x -> 0x%x 0x%x", pa, va, len);
	r = addr_map(pa, va, len, MAP_TABLE);
	log(LOG_INFO, "l2 map got %i", r);
	return r;
}

