#include "head.h"
#include <mmu.h>

struct kernel_info *info;
int main_eid;

void
log(int level, char *fmt, ...)
{
#if 1
	char buf[256];

	va_list a;
	va_start(a, fmt);
	vsnprintf(buf, sizeof(buf),
			fmt, a);
	va_end(a);

	kern_debug(buf);
#endif
}

	int
handle_addr_req(int eid, int from, union proc0_req *rq)
{
	struct addr_frame *f;
	union proc0_rsp rp;
	size_t pa, len;

	rp.addr_req.type = PROC0_addr_req_rsp;

	len = rq->addr_req.len;
	if (len != PAGE_ALIGN(len)) {
		rp.addr_req.ret = PROC0_ERR_ALIGNMENT;
		return reply(eid, from, &rp);	
	}
	
	pa = rq->addr_req.pa;
	if (pa != PAGE_ALIGN(pa)) {
		rp.addr_req.ret = PROC0_ERR_ALIGNMENT;
		return reply(eid, from, &rp);	
	}

	pa = get_ram(len, 0x1000);
	if (pa == nil) {
		rp.addr_req.ret = PROC0_ERR_ALIGNMENT;
		return reply(eid, from, &rp);	
	}

	rp.addr_req.pa = pa;

	f = frame_new(pa, len);
	if (f == nil) {
		rp.addr_req.ret = ERR;
		return reply(eid, from, &rp);
	}

	rp.addr_req.ret = proc_give_addr(from, f);
	return reply(eid, from, &rp);
}

	int
handle_addr_map(int eid, int from, union proc0_req *rq)
{
	size_t pa, va, len;
	union proc0_rsp rp;

	log(0, "proc %i wants to map 0x%x 0x%x to 0x%x",
		from, rq->addr_map.pa, rq->addr_map.len,
		rq->addr_map.va);

	rp.addr_map.type = PROC0_addr_map_rsp;

	len = rq->addr_map.len;
	if (len != PAGE_ALIGN(len)) {
		rp.addr_map.ret = PROC0_ERR_ALIGNMENT;
		return reply(eid, from, &rp);
	}
	
	pa = rq->addr_map.pa;
	if (pa != PAGE_ALIGN(pa)) {
		rp.addr_map.ret = PROC0_ERR_ALIGNMENT;
		return reply(eid, from, &rp);
	}

	va = rq->addr_map.va;
	if (va != PAGE_ALIGN(va)) {
		rp.addr_map.ret = PROC0_ERR_ALIGNMENT;
		return reply(eid, from, &rp);
	}

	rp.addr_map.ret = proc_map(from, 
			pa, va, len, 
			rq->addr_map.flags);
		
	return reply(eid, from, &rp);
}

	int
handle_addr_give(int eid, int from, union proc0_req *rq)
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
		return reply(eid, from, &rp);
	}
	
	pa = rq->addr_give.pa;
	if (pa != PAGE_ALIGN(pa)) {
		rp.addr_give.ret = PROC0_ERR_ALIGNMENT;
		return reply(eid, from, &rp);
	}

	f = proc_take_addr(from, pa, len);
	if (f == nil) {
		rp.addr_give.ret = ERR;
		return reply(eid, from, &rp);
	}

	if (to == ROOT_PID) {
		frame_free(f);
		rp.addr_give.ret = OK;

	} else {
		rp.addr_give.ret = proc_give_addr(to, f);
	}
	
	return reply(eid, from, &rp);
}

struct service *
find_service_pid(int pid)
{
	int i;

	for (i = 0; i < nservices; i++) {
		if (services[i].pid == pid) {
			return &services[i];
		}
	}

	return nil;
}

struct service *
find_service_resource(struct service *s, int type)
{
	char *name = nil;
	int i;

	for (i = 0; i < 32; i++) {
		if (s->resources[i].type == RESOURCE_type_none) {
			return nil;
		} else if (s->resources[i].type == type) {
			name = s->resources[i].name;
			break;
		}
	}

	for (i = 0; i < nservices; i++) {
		if (strncmp(services[i].name, name, sizeof(services[i].name))) {
			return &services[i];
		}
	}

	return nil;
}

	int
handle_get_resource(int eid, int from, union proc0_req *rq)
{
	union proc0_rsp rp;
	struct addr_frame *f;
	struct service *s, *r;

	log(0, "get resource from %i", from);

	rp.get_resource.type = PROC0_get_resource_rsp;
	rp.get_resource.ret = ERR;

	s = find_service_pid(from);
	if (s == nil) {
		log(0, "proc %i not a service", from);
		rp.get_resource.ret = ERR;
		return reply(eid, from, &rp);
	}

	switch (rq->get_resource.resource_type) {
	default:
		rp.get_resource.ret = ERR;
		break;

	case RESOURCE_type_log:
	case RESOURCE_type_timer:
	case RESOURCE_type_block:
	case RESOURCE_type_net:
	case RESOURCE_type_serial:
		r = find_service_resource(s, rq->get_resource.resource_type);
		if (r != nil) {
			endpoint_offer(r->eid);

			rp.get_resource.ret = OK;
		} else {
			rp.get_resource.ret = ERR;
		}
		break;
	
	case RESOURCE_type_int:
		if (s->device.is_device && !s->device.has_irq) {
			log(0, "giving proc %i its int %i", 
				from, s->device.irqn);

			s->device.has_irq = true;

			rp.get_resource.result.irqn = s->device.irqn;
			rp.get_resource.ret = OK;
		} else {
			rp.get_resource.ret = ERR;
		}

		break;
	
	case RESOURCE_type_regs:
		if (s->device.is_device && !s->device.has_regs) {
			log(0, "giving proc %i its regs 0x%x 0x%x",
				from, s->device.reg, s->device.len);

			s->device.has_regs = true;

			f = frame_new(PAGE_ALIGN_DN(s->device.reg),
					PAGE_ALIGN(s->device.len));

			proc_give_addr(from, f);

			rp.get_resource.result.regs.pa = s->device.reg;
			rp.get_resource.result.regs.len = s->device.len;
			rp.get_resource.ret = OK;
		} else {
			rp.get_resource.ret = ERR;
		}

		break;
	}

	return reply(eid, from, &rp);
}

#if 0
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

#endif

	void
main(struct kernel_info *i)
{
	uint8_t m[MESSAGE_LEN];
	int eid, from;

	info = i;

	log(0, "kernel_info at 0x%x", i);
	log(0, "kernel starts at 0x%x", info->kernel_va);
	log(0, "boot starts at 0x%x", info->boot_pa);

	main_eid = endpoint_create();
	if (main_eid < 0) {
		log(0, "ERROR creating main endpoint %i", main_eid);
		exit(1);
	}

	init_mem();
	init_procs();

	while (true) {
		if ((eid = recv(main_eid, &from, m)) < 0) continue;
		if (from == PID_SIGNAL) continue;

		log(0, "got message from %i on eid %i of type 0x%x",
				from, eid, ((uint32_t *) m)[0]);

		switch (((uint32_t *) m)[0]) {
			case PROC0_addr_req_req:
				handle_addr_req(eid, from, (union proc0_req *) m);
				break;

			case PROC0_addr_map_req:
				handle_addr_map(eid, from, (union proc0_req *) m);
				break;

			case PROC0_addr_give_req:
				handle_addr_give(eid, from, (union proc0_req *) m);
				break;
			
			case PROC0_get_resource_req:
				handle_get_resource(eid, from, (union proc0_req *) m);
				break;

/*
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
*/
		};
	}
}

