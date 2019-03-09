#include "head.h"
#include "../dev.h"

struct kernel_info *info;

	int
handle_addr_req(int from, union proc0_req *rq, union proc0_rsp *rp)
{
	size_t pa, len;

	len = rq->addr_req.len;
	if (len != PAGE_ALIGN(len)) {
		return ERR;
	}
	
	pa = rq->addr_req.pa;
	if (pa != PAGE_ALIGN(pa)) {
		return ERR;
	}

	pa = get_ram(len, 0x1000);
	if (pa == nil) {
		return ERR;
	}

	rp->addr_req.pa = pa;

	return proc_give_addr(from, pa, len);
}

	int
handle_addr_unmap(int from, union proc0_req *rq, union proc0_rsp *rp)
{
	size_t va, len;

	len = rq->addr_unmap.len;
	if (len != PAGE_ALIGN(len)) {
		return ERR;
	}
	
	va = rq->addr_unmap.va;
	if (va != PAGE_ALIGN(va)) {
		return ERR;
	}

	return proc_unmap(from, va, len);
}

	int
handle_addr_map(int from, union proc0_req *rq, union proc0_rsp *rp)
{
	size_t pa, va, len;

	len = rq->addr_map.len;
	if (len != PAGE_ALIGN(len)) {
		return ERR;
	}
	
	pa = rq->addr_map.pa;
	if (pa != PAGE_ALIGN(pa)) {
		return ERR;
	}

	va = rq->addr_map.va;
	if (va != PAGE_ALIGN(va)) {
		return ERR;
	}

	return proc_map(from, 
			pa, va, len, 
			rq->addr_map.flags);
}

	int
handle_addr_give(int from, union proc0_req *rq, union proc0_rsp *rp)
{
	size_t pa, len;
	int to, ret;

	to = rq->addr_give.to;

	len = rq->addr_give.len;
	if (len != PAGE_ALIGN(len)) {
		return ERR;
	}
	
	pa = rq->addr_give.pa;
	if (pa != PAGE_ALIGN(pa)) {
		return ERR;
	}

	/* TODO: Make sure it is not mapped */

	ret = proc_take_addr(from, pa, len);
	if (ret != OK) {
		return ret;
	}

	if (to == PROC0_PID) {
		free_addr(pa, len);
		return OK;

	} else {
		return proc_give_addr(to, pa, len);
	}
}

static int irq_owners[256] = { -1 };

int
handle_irq_reg(int from, union proc0_req *rq, union proc0_rsp *rp)
{
	size_t irqn;

	irqn = rq->irq_reg.irqn;

	if (irq_owners[irqn] == -1) {
		irq_owners[irqn] = from;
		return intr_register(from, irqn);
	} else {
		return ERR;
	}
}

	int
handle_proc(int from, union proc0_req *rq, union proc0_rsp *rp)
{
	return ERR;
}

	void
main(struct kernel_info *i)
{
	uint8_t rq_buf[MESSAGE_LEN], rp_buf[MESSAGE_LEN];
	union proc0_req *rq = (union proc0_req *) rq_buf;
	union proc0_rsp *rp = (union proc0_rsp *) rp_buf;
	int from;

	info = i;

	init_pools();	
	init_mem();
	init_procs();

	while (true) {
		if ((from = recv(-1, rq_buf)) < 0)
			continue;

		rp->untyped.type = rq->type;

		switch (rq->type) {
			case PROC0_addr_req:
				rp->untyped.ret = handle_addr_req(from, rq, rp);
				break;

			case PROC0_addr_map:
				rp->untyped.ret = handle_addr_map(from, rq, rp);
				break;

			case PROC0_addr_unmap:
				rp->untyped.ret = handle_addr_unmap(from, rq, rp);
				break;

			case PROC0_addr_give:
				rp->untyped.ret = handle_addr_give(from, rq, rp);
				break;

			case PROC0_irq_reg:
				rp->untyped.ret = handle_irq_reg(from, rq, rp);
				break;

			case PROC0_proc:
				rp->untyped.ret = handle_proc(from, rq, rp);
				break;

			default:
				rp->untyped.ret = ERR;
				break;
		};

		send(from, rp_buf);
	}
}

