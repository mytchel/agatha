#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <arm/mmu.h>
#include <proc0.h>

	int
request_memory(size_t len, size_t align)
{
	union proc0_req rq;
	union proc0_rsp rp;
	int cid;

	if (len == 0) {
		return nil;
	}

	cid = kcap_alloc();
	if (cid < 0) {
		return ERR;
	}

	len = PAGE_ALIGN(len);

	rq.mem_req.type = PROC0_mem_req;
	rq.mem_req.len = len;
	rq.mem_req.align = align;

	if (mesg_cap(CID_PARENT, &rq, &rp, cid) != OK) {
		return ERR;
	}

	if (rp.mem_req.ret != OK) {
		return rp.mem_req.ret;
	}

	return cid;
}

	int
release_memory(int cid)
{
	return ERR;
}

