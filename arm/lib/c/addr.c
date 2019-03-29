#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <arm/mmu.h>
#include <proc0.h>
#include <log.h>

/*#define log(X, ...) */

extern uint32_t *_data_end;
static size_t _heap_end = 0;

int
unmap_addr(void *va, size_t len)
{
	union proc0_req rq;
	union proc0_rsp rp;

	if (PAGE_ALIGN(va) != (size_t) va) return nil;
	
	len = PAGE_ALIGN(len);

	rq.addr_unmap.type = PROC0_addr_unmap;
	rq.addr_unmap.len = len;
	rq.addr_unmap.va = (size_t) va;

	if (mesg(PROC0_PID, &rq, &rp) != OK) {
		return nil;
	}

	return rp.addr_unmap.ret;
}

static int
prepare_l2s(size_t to)
{
	size_t bottom, top, tables, pa;
	union proc0_req rq;
	union proc0_rsp rp;

	log(LOG_INFO, "adding extra l2 to get from 0x%x to 0x%x", _heap_end, to);

	bottom = TABLE_ALIGN(_heap_end);
	top = TABLE_ALIGN(to);

	tables = ((top - bottom) >> TABLE_SHIFT) + 1;

	log(LOG_INFO, "need %i tables to cover 0x%x to 0x%x", tables, bottom, top);

	pa = request_memory(tables * TABLE_SIZE);
	if (pa == nil) {
		log(LOG_INFO, "failed to get memory");
		return ERR;
	}

	rq.addr_map.type = PROC0_addr_map;
	rq.addr_map.pa = pa;
	rq.addr_map.len = tables * TABLE_SIZE;
	rq.addr_map.va = bottom;
	rq.addr_map.flags = MAP_TABLE;

	if (mesg(PROC0_PID, &rq, &rp) != OK) {
		log(LOG_INFO, "mesg failed");
		return ERR;
	} else if (rp.addr_req.ret != OK) {
		log(LOG_INFO, "proc0 denied %i", rp.addr_req.ret);
		return ERR;
	}

	log(LOG_INFO, "new l2 table added");
	return OK;
}

void *
map_addr(size_t pa, size_t len, int flags)
{
	union proc0_req rq;
	union proc0_rsp rp;
	size_t va;

	log(LOG_INFO, "mapping 0x%x 0x%x 0x%x", pa, len, flags);

	if (PAGE_ALIGN(pa) != pa) return nil;
	
	len = PAGE_ALIGN(len);

	if (_heap_end == 0)
		_heap_end = PAGE_ALIGN((size_t) &_data_end);

	if (L1X(_heap_end) < L1X(_heap_end + len)) {
		if (prepare_l2s(_heap_end + len) != OK) {
			return nil;
		}	
	}

	log(LOG_INFO, "ask proc0 to map");

	va = _heap_end;
	_heap_end += len;

	rq.addr_map.type = PROC0_addr_map;
	rq.addr_map.pa = pa;
	rq.addr_map.len = len;
	rq.addr_map.va = va;
	rq.addr_map.flags = flags;

	if (mesg(PROC0_PID, &rq, &rp) != OK) {
		return nil;
	}

	if (rp.addr_req.ret != OK) {
		log(LOG_INFO, "proc0 denied %i", rp.addr_req.ret);
		return nil;
	}

	log(LOG_INFO, "mapped addr");

	return (void *) va;
}

size_t
request_memory(size_t len)
{
	union proc0_req rq;
	union proc0_rsp rp;

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

	rq.addr_give.type = PROC0_addr_give;
	rq.addr_give.to = to;
	rq.addr_give.pa = pa;
	rq.addr_give.len = len;

	if (mesg(PROC0_PID, &rq, &rp) != OK) {
		return ERR;
	}

	return rp.addr_give.ret;
}

