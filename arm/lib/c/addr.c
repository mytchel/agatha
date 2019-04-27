#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <arm/mmu.h>
#include <proc0.h>
#include <log.h>

extern uint32_t *_data_end;
static size_t _heap_end = 0;

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
unmap_addr(void *va, size_t len)
{
	union proc0_req rq;
	union proc0_rsp rp;

	log(LOG_INFO, "unmap 0x%x . 0x%x", va, len);

	if (PAGE_ALIGN(va) != (size_t) va) {
		return nil;
	}
	
	len = PAGE_ALIGN(len);

	rq.addr_map.type = PROC0_addr_map;
	rq.addr_map.len = len;
	rq.addr_map.va = (size_t) va;
	rq.addr_map.pa = 0;
	rq.addr_map.flags = MAP_REMOVE_LEAF;

	if (mesg(PROC0_PID, &rq, &rp) != OK) {
		return ERR;
	}

	log(LOG_INFO, "unmap got %i", rp.addr_map.ret);

	return rp.addr_map.ret;
}

static int
map_to(size_t pa, size_t va, size_t len, int flags)
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

static int
prepare_l2s(size_t to)
{
	size_t bottom, top, tables, pa;

	log(LOG_INFO, "add l2 to cover 0x%x", to);

	bottom = TABLE_ALIGN(_heap_end);
	top = TABLE_ALIGN(to);

	tables = ((top - bottom) >> TABLE_SHIFT) + 1;

	pa = request_memory(tables * TABLE_SIZE);
	if (pa == nil) {
		return ERR;
	}

	log(LOG_INFO, "map l2 at 0x%x %i tables", bottom, tables);
	int r = map_to(pa, bottom, tables * TABLE_SIZE, MAP_TABLE);
	log(LOG_INFO, "l2 map got %i", r);
	return r;
}

size_t
take_free_addr(size_t len)
{
	size_t va;

	if (_heap_end == 0)
		_heap_end = PAGE_ALIGN((size_t) &_data_end);

	if (L1X(_heap_end) < L1X(_heap_end + len)) {
		if (prepare_l2s(_heap_end + len) != OK) {
			return nil;
		}	
	}

	va = _heap_end;
	_heap_end += len;

	return va;
}

void *
map_addr(size_t pa, size_t len, int flags)
{
	size_t va;

	log(LOG_INFO, "map 0x%x 0x%x", pa, len);

	if (PAGE_ALIGN(pa) != pa) return nil;
	
	len = PAGE_ALIGN(len);

	va = take_free_addr(len);
	if (va == nil) {
		return nil;

	} else if (map_to(pa, va, len, flags) != OK) {
		return nil;

	} else {
		return (void *) va;
	}
}


