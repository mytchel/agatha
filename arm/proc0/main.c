#include "head.h"
#include "../dev.h"

struct kernel_info *info;

	int
handle_mem_req(struct proc0_req *rq, struct proc0_rsp *rp)
{
	size_t pa, va, len;

	len = PAGE_ALIGN(rq->m.mem_req.len);
	
	pa = rq->m.mem_req.pa;
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

	va = proc_map(rq->from, pa, rq->m.mem_req.va, len, 
			rq->m.mem_req.flags);
	if (va == nil) {
		free_mem(pa, len);
		return ERR;
	}

	rp->m.mem_req.addr = va;

	return OK;
}

	int
handle_irq_req(struct proc0_req *rq, struct proc0_rsp *rp)
{
	return intr_register(rq->from, rq->m.irq_req.irqn);
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
			case PROC0_mem_req:
				rp->ret = handle_mem_req(rq, rp);
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

