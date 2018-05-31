#include "head.h"
#include "../dev.h"

struct kernel_info *info;

	int
handle_addr_req(struct proc0_req *rq, struct proc0_rsp *rp)
{
	size_t pa, len;

	len = rq->m.addr_req.len;
	if (len != PAGE_ALIGN(len)) {
		return ERR;
	}
	
	pa = rq->m.addr_req.pa;
	if (pa != PAGE_ALIGN(pa)) {
		return ERR;
	}

	if (pa == nil) {
		pa = get_ram(len, 0x1000);
		if (pa == nil) {
			return ERR;
		}
	} else {
		if (get_regs(pa, len) != OK) {
			return ERR;
		}
	}

	rp->m.addr_req.pa = pa;

	return proc_give(rq->from, pa, len);
}

	int
handle_addr_map(struct proc0_req *rq, struct proc0_rsp *rp)
{
	size_t pa, va, len;

	len = rq->m.addr_map.len;
	if (len != PAGE_ALIGN(len)) {
		return ERR;
	}
	
	pa = rq->m.addr_map.pa;
	if (pa != PAGE_ALIGN(pa)) {
		return ERR;
	}

	va = rq->m.addr_map.va;
	if (va != PAGE_ALIGN(va)) {
		return ERR;
	}

	return proc_map(rq->from, 
			pa, va, len, 
			rq->m.addr_map.flags);
}

static int irq_owners[256] = { -1 };

int
handle_irq_reg(struct proc0_req *rq, struct proc0_rsp *rp)
{
	size_t irqn;

	irqn = rq->m.irq_reg.irqn;

	if (irq_owners[irqn] == -1) {
		irq_owners[irqn] = rq->from;
		return OK;
	} else {
		return ERR;
	}
}

	int
handle_irq_req(struct proc0_req *rq, struct proc0_rsp *rp)
{
	size_t irqn;

	irqn = rq->m.irq_req.irqn;

	if (irq_owners[irqn] == rq->from) {
		return intr_register(rq->from, rq->m.irq_req.irqn);
	} else {
		return ERR;
	}
}

	int
handle_proc(struct proc0_req *rq, struct proc0_rsp *rp)
{
	return ERR;
}

	void
main(struct kernel_info *i)
{
	uint8_t rq_buf[MESSAGE_LEN], rp_buf[MESSAGE_LEN];
	struct proc0_req *rq = (struct proc0_req *) rq_buf;
	struct proc0_rsp *rp = (struct proc0_rsp *) rp_buf;

	info = i;

	init_pools();	
	init_mem();
	init_procs();

	while (true) {
		if (recv(rq_buf) != OK) continue;

		rp->from = 0;
		rp->type = rq->type;

		switch (rq->type) {
			case PROC0_addr_req:
				rp->ret = handle_addr_req(rq, rp);
				break;

			case PROC0_addr_map:
				rp->ret = handle_addr_map(rq, rp);
				break;

			case PROC0_irq_reg:
				rp->ret = handle_irq_reg(rq, rp);
				break;

			case PROC0_irq_req:
				rp->ret = handle_irq_req(rq, rp);
				break;

			case PROC0_proc:
				rp->ret = handle_proc(rq, rp);
				break;

			default:
				rp->ret = ERR;
				break;
		};

		send(rq->from, rp_buf);
	}
}

