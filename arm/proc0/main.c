#include "head.h"
#include <mmu.h>
#include "../dev.h"

struct kernel_info *info;

void
log(int level, char *fmt, ...)
{
	/*
	char buf[256];
	va_list a;

	va_start(a, fmt);
	vsnprintf(buf, sizeof(buf),
			fmt, a);
	va_end(a);

	kern_debug(buf);
	*/
}

	int
handle_addr_req(int from, union proc0_req *rq)
{
	struct addr_frame *f;
	union proc0_rsp rp;
	size_t pa, len;

	rp.addr_req.type = PROC0_addr_req_rsp;

	len = rq->addr_req.len;
	if (len != PAGE_ALIGN(len)) {
		rp.addr_req.ret = PROC0_ERR_ALIGNMENT;
		return send(from, &rp);	
	}
	
	pa = rq->addr_req.pa;
	if (pa != PAGE_ALIGN(pa)) {
		rp.addr_req.ret = PROC0_ERR_ALIGNMENT;
		return send(from, &rp);	
	}

	pa = get_ram(len, 0x1000);
	if (pa == nil) {
		rp.addr_req.ret = PROC0_ERR_ALIGNMENT;
		return send(from, &rp);	
	}

	rp.addr_req.pa = pa;

	f = frame_new(pa, len);
	if (f == nil) {
		rp.addr_req.ret = ERR;
		return send(from, &rp);
	}

	rp.addr_req.ret = proc_give_addr(from, f);
	return send(from, &rp);
}

	int
handle_addr_map(int from, union proc0_req *rq)
{
	size_t pa, va, len;
	union proc0_rsp rp;

	rp.addr_map.type = PROC0_addr_map_rsp;

	len = rq->addr_map.len;
	if (len != PAGE_ALIGN(len)) {
		rp.addr_map.ret = PROC0_ERR_ALIGNMENT;
		return send(from, &rp);
	}
	
	pa = rq->addr_map.pa;
	if (pa != PAGE_ALIGN(pa)) {
		rp.addr_map.ret = PROC0_ERR_ALIGNMENT;
		return send(from, &rp);
	}

	va = rq->addr_map.va;
	if (va != PAGE_ALIGN(va)) {
		rp.addr_map.ret = PROC0_ERR_ALIGNMENT;
		return send(from, &rp);
	}

	rp.addr_map.ret = proc_map(from, 
			pa, va, len, 
			rq->addr_map.flags);
		
	return send(from, &rp);
}

	int
handle_addr_give(int from, union proc0_req *rq)
{
	union proc0_rsp rp;
	struct addr_frame *f;
	size_t pa, len;
	int to;

	rp.addr_give.type = PROC0_addr_give_rsp;

	to = rq->addr_give.to;

	len = rq->addr_give.len;
	if (len != PAGE_ALIGN(len)) {
		rp.addr_give.ret = PROC0_ERR_ALIGNMENT;
		return send(from, &rp);
	}
	
	pa = rq->addr_give.pa;
	if (pa != PAGE_ALIGN(pa)) {
		rp.addr_give.ret = PROC0_ERR_ALIGNMENT;
		return send(from, &rp);
	}

	f = proc_take_addr(from, pa, len);
	if (f == nil) {
		rp.addr_give.ret = ERR;
		return send(from, &rp);
	}

	if (to == PROC0_PID) {
		frame_free(f);
		rp.addr_give.ret = OK;

	} else {
		rp.addr_give.ret = proc_give_addr(to, f);
	}
	
	return send(from, &rp);
}

int
handle_irq_reg(int from, union proc0_req *rq)
{
	struct intr_mapping map;
	union proc0_rsp rp;

	map.pid = from;
	map.irqn = rq->irq_reg.irqn;
	map.func = rq->irq_reg.func;
	map.arg = rq->irq_reg.arg;
	map.sp = rq->irq_reg.sp;

	rp.irq_reg.type = PROC0_irq_reg_rsp;
	rp.irq_reg.ret = intr_register(&map);
	return send(from, &rp);
}

	int
handle_proc(int from, union proc0_req *rq)
{
	union proc0_rsp rp;

	rp.proc.type = PROC0_proc_rsp;
	rp.proc.ret = ERR;
	return send(from, &rp);
}

int
handle_fault(int from, union proc_msg *m)
{
	log(0, "proc %i fault flags %i at pc 0x%x", 
			from, m->fault.fault_flags, m->fault.pc);

	if ((m->fault.fault_flags & 3) == 3) {
		log(0, "for addr 0x%x", m->fault.data_addr);
	}

	return OK;
}

int
handle_exit(int from, union proc_msg *m)
{
	log(0, "proc %i exit with 0x%x",
			from, m->exit.code);

	return OK;
}

	void
main(struct kernel_info *i)
{
	uint8_t m[MESSAGE_LEN];
	int from;

	info = i;

	log(0, "kernel_info at 0x%x", i);
	log(0, "kernel starts at 0x%x", info->kernel_va);
	log(0, "boot starts at 0x%x", info->boot_pa);

	init_mem();
	init_procs();

	while (true) {
		if ((from = recv(-1, m)) < 0)
			continue;

		switch (((uint32_t *) m)[0]) {
			case PROC0_addr_req_req:
				handle_addr_req(from, (union proc0_req *) m);
				break;

			case PROC0_addr_map_req:
				handle_addr_map(from, (union proc0_req *) m);
				break;

			case PROC0_addr_give_req:
				handle_addr_give(from, (union proc0_req *) m);
				break;

			case PROC0_irq_reg_req:
				handle_irq_reg(from, (union proc0_req *) m);
				break;

			case PROC0_proc_req:
				handle_proc(from, (union proc0_req *) m);
				break;

			case PROC_fault_msg:
				handle_fault(from, (union proc_msg *) m);
				break;

			case PROC_exit_msg:
				handle_exit(from, (union proc_msg *) m);
				break;
		};
	}
}

