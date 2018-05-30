#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <m.h>
#include <stdarg.h>
#include <string.h>

#include "../dev.h"
#include "head.h"

struct kernel_info *info;

	int
handle_mem_req(struct proc0_req *rq, struct proc0_rsp *rp)
{
	size_t pa, va, len;

	len = PAGE_ALIGN(rq->m.mem_req.len);

	pa = get_mem(len, 0x1000);
	if (pa == nil) {
		return ERR;
	}

	va = proc_map(rq->from, pa, rq->m.mem_req.addr, len, 
			rq->m.mem_req.flags);
	if (va == nil) {
		free_mem(pa, len);
		return ERR;
	}

	rp->m.mem_req.addr = va;

	return OK;
}

int
handle_dev_register(struct proc0_req *rq, struct proc0_rsp *rp)
{
	int i;

	for (i = 0; i < ndevices; i++) {
		if (strncmp(devices[i].compatable, 
					rq->m.dev_register.compat, 
					sizeof(rq->m.dev_register.compat))) {

			rp->m.dev_register.id = i;
			return OK;
		}
	}

	return ERR;
}

int
handle_dev_req(struct proc0_req *rq, struct proc0_rsp *rp)
{
	size_t pa, va, len;
	int flags;

	if (rq->m.dev_req.n > 0) {
		return ERR;
	}

	rp->m.dev_req.irq = devices[rq->m.dev_req.id].irqn;

	len = PAGE_ALIGN(devices[rq->m.dev_req.id].len);
	pa = devices[rq->m.dev_req.id].reg;

	flags = MAP_DEV|MAP_RW;
	va = proc_map(rq->from, pa, nil, len, flags);
	if (va == nil) {
		return ERR;
	}

	rp->m.dev_req.addr = va;
	rp->m.dev_req.len = len;

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

		case PROC0_dev_register:
			rp->ret = handle_dev_register(rq, rp);
			break;

		case PROC0_dev_req:
			rp->ret = handle_dev_req(rq, rp);
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

