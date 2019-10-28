#include "head.h"
#include <mmu.h>
#include <log.h>

struct kernel_info *info = nil;
int main_eid = -1;

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
handle_mem_req(int eid, int from, union proc0_req *rq)
{
	union proc0_rsp rp;
	size_t len, align;
	int cid;

	rp.mem_req.type = PROC0_mem_req;

	len = rq->mem_req.len;
	if (len != PAGE_ALIGN(len)) {
		rp.mem_req.ret = PROC0_ERR_ALIGNMENT;
		return reply(eid, from, &rp);	
	}
	
	align = rq->mem_req.align;
	if (align != PAGE_ALIGN(align)) {
		rp.mem_req.ret = PROC0_ERR_ALIGNMENT;
		return reply(eid, from, &rp);	
	}

	cid = request_memory(len, align);
	if (cid < 0) {
		rp.mem_req.ret = PROC0_ERR_ALIGNMENT;
		return reply(eid, from, &rp);	
	}

	rp.mem_req.ret = OK;
	return reply_cap(eid, from, &rp, cid);
}

#if 0
	int
handle_addr_map(int eid, int from, union proc0_req *rq)
{
	size_t pa, va, len;
	union proc0_rsp rp;

	log(0, "proc %i wants to map 0x%x 0x%x to 0x%x",
		from, rq->addr_map.pa, rq->addr_map.len,
		rq->addr_map.va);

	rp.addr_map.type = PROC0_addr_map;

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

	rp.addr_give.type = PROC0_addr_give;

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
#endif

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
	struct service *s, *r;
	union proc0_rsp rp;
	int give_cap;

	give_cap = 0;

	log(0, "get resource from %i", from);

	rp.get_resource.type = PROC0_get_resource;
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

	case RESOURCE_type_mount:
		if (s->listen_eid >= 0) {
			log(0, "give proc %i mount endpoint %i", from, s->listen_eid);

			give_cap = s->listen_eid;
			s->listen_eid = 0;
			
			rp.get_resource.ret = OK;
		} else {
			log(0, "cannot give proc %i mount endpoint", from);
			rp.get_resource.ret = ERR;
		}

		break;

	case RESOURCE_type_log:
	case RESOURCE_type_timer:
	case RESOURCE_type_block:
	case RESOURCE_type_net:
	case RESOURCE_type_serial:
		r = find_service_resource(s, rq->get_resource.resource_type);
		if (r != nil) {
			log(0, "giving proc %i resource type %i eid %i pid %i",
				from, rq->get_resource.resource_type, r->connect_eid, r->pid);

			give_cap = kcap_alloc();

			rp.get_resource.ret = endpoint_connect(r->connect_eid, give_cap);
		} else {
			rp.get_resource.ret = ERR;
		}
		break;
	
	case RESOURCE_type_int:
		if (s->device.is_device && !s->device.has_irq) {
			log(0, "giving proc %i its int %i, cap id %i", 
				from, s->device.irqn, s->device.irq_cid);

			s->device.has_irq = true;

			give_cap = s->device.irq_cid;

			rp.get_resource.result.irqn = s->device.irqn;
			rp.get_resource.ret = OK;
		} else {
			rp.get_resource.ret = ERR;
		}

		break;
	
	case RESOURCE_type_regs:
		rp.get_resource.ret = ERR;
#if 0
		return reply
		if (s->device.is_device && !s->device.has_regs) {
			log(0, "giving proc %i its regs 0x%x 0x%x",
				from, s->device.reg, s->device.len);

			s->device.has_regs = true;

			proc_give_addr(from, s->device.reg_frame);

			rp.get_resource.result.regs.pa = s->device.reg;
			rp.get_resource.result.regs.len = s->device.len;
			rp.get_resource.ret = OK;
		} else {
			rp.get_resource.ret = ERR;
		}
#endif
		break;
	}

	return reply_cap(eid, from, &rp, give_cap);
}

	void
main(struct kernel_info *i)
{
	uint8_t m[MESSAGE_LEN];
	int eid, from;

	info = i;

	log(0, "kernel_info at 0x%x", i);
	log(0, "kernel starts at 0x%x", info->kernel_va);
	log(0, "boot starts at 0x%x", info->boot_pa);

	if (kobj_add_untyped(CID_L1+1, 0x1000) != OK) {
		log(LOG_WARNING, "error adding initial object");
		exit(1);
	}

	init_mem();

	main_eid = kobj_alloc(OBJ_endpoint, 1);
	if (main_eid < 0) {
		log(LOG_WARNING, "error creating main endpoint %i", main_eid);
		exit(1);
	}

	init_procs();

	while (true) {
		if ((eid = recv(main_eid, &from, m)) < 0) continue;
		if (from == PID_SIGNAL) continue;

		log(0, "got message from %i on eid %i of type 0x%x",
				from, eid, ((uint32_t *) m)[0]);

		switch (((uint32_t *) m)[0]) {
			case PROC0_mem_req:
				handle_mem_req(eid, from, (union proc0_req *) m);
				break;

#if 0
			case PROC0_addr_map:
				handle_addr_map(eid, from, (union proc0_req *) m);
				break;

			case PROC0_addr_give:
				handle_addr_give(eid, from, (union proc0_req *) m);
				break;
#endif
			
			case PROC0_get_resource:
				handle_get_resource(eid, from, (union proc0_req *) m);
				break;
		};
	}
}

