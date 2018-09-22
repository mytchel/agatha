#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <proc0.h>

extern uint32_t *_data_end;
static size_t _heap_end = 0;

void *
request_memory(size_t len, int flags)
{
	union proc0_req rq;
	union proc0_rsp rp;
	size_t va, pa;

	len = PAGE_ALIGN(len);

	rq.addr_req.type = PROC0_addr_req;
	rq.addr_req.pa = nil;
	rq.addr_req.len = len;

	if (send(PROC0_PID, (uint8_t *) &rq) != OK) {
		return nil;
	}

	while (recv(0, (uint8_t *) &rp) != 0)
		;

	if (rp.addr_req.ret != OK) {
		return nil;
	}

	pa = rp.addr_req.pa;

	if (_heap_end == 0)
		_heap_end = PAGE_ALIGN((size_t) &_data_end);

	va = _heap_end;
	_heap_end += len;

	rq.addr_map.type = PROC0_addr_map;
	rq.addr_map.pa = pa;
	rq.addr_map.len = len;
	rq.addr_map.va = va;
	rq.addr_map.flags = flags;

	if (send(PROC0_PID, (uint8_t *) &rq) != OK) {
		/* TODO: release memory? */
		return nil;
	}

	while (recv(0, (uint8_t *) &rp) != 0)
		;

	if (rp.addr_req.ret != OK) {
		/* TODO: release memory? */
		return nil;
	}

	return (void *) va;
}

void *
request_device(size_t pa, size_t len, int flags)
{
	union proc0_req rq;
	union proc0_rsp rp;
	size_t va;

	len = PAGE_ALIGN(len);

	rq.addr_req.type = PROC0_addr_req;
	rq.addr_req.pa = pa;
	rq.addr_req.len = len;

	if (send(PROC0_PID, (uint8_t *) &rq) != OK) {
		return nil;
	}

	if (recv(PROC0_PID, (uint8_t *) &rp) != OK) {
		return nil;
	}

	if (rp.addr_req.ret != OK) {
		return nil;
	}

	if (_heap_end == 0)
		_heap_end = PAGE_ALIGN((size_t) &_data_end);

	va = _heap_end;
	_heap_end += len;

	rq.addr_map.type = PROC0_addr_map;
	rq.addr_map.pa = pa;
	rq.addr_map.len = len;
	rq.addr_map.va = va;
	rq.addr_map.flags = flags;

	if (send(PROC0_PID, (uint8_t *) &rq) != OK) {
		/* TODO: release memory? */
		return nil;
	}

	if (recv(PROC0_PID, (uint8_t *) &rp) != OK) {
		return nil;
	}

	if (rp.addr_map.ret != OK) {
		/* TODO: release memory? */
		return nil;
	}

	return (void *) va;
}

int
release_addr(void *start, size_t len)
{
	/* TODO */
	return ERR;
}

int
give_addr(int to, size_t pa, size_t len)
{
	union proc0_req rq;
	union proc0_rsp rp;

	rq.addr_give.type = PROC0_addr_req;
	rq.addr_give.to = to;
	rq.addr_give.pa = pa;
	rq.addr_give.len = len;

	if (send(PROC0_PID, (uint8_t *) &rq) != OK) {
		return nil;
	}

	if (recv(PROC0_PID, (uint8_t *) &rp) != OK) {
		return nil;
	}

	return rp.addr_give.ret;
}

