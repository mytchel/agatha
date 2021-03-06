#include "head.h"
#include <arm/mmu.h>
#include "../bundle.h"
#include <log.h>

int
init_bundled_proc(uint8_t *code, 
		char *name, int priority,
		size_t len)
{
	int p_cid, eid, clist;
	int l1, l2;
	int code_new;
	int stack;
	void *code_va;

	log(LOG_INFO, "init bundled %s", name);

	log(LOG_INFO, "get code mem 0x%x", len);

	code_new = request_memory(len, 0x1000);
	if (code_new < 0) {
		log(LOG_WARNING, "out of mem for code");
		exit(1);
	}

	log(LOG_INFO, "get stack mem 0x%x", 0x1000);

	stack = request_memory(0x1000, 0x1000);
	if (stack < 0) {
		log(LOG_WARNING, "out of mem for stack");
		exit(1);
	}
	
	log(LOG_INFO, "get l1 mem 0x%x", 0x4000);

	l1 = request_memory(0x4000, 0x4000);
	if (l1 < 0) {
		log(LOG_WARNING, "out of mem for l1");
		exit(1);
	}

	log(LOG_INFO, "get l2 mem 0x%x", 0x1000);

	l2 = request_memory(0x1000, 0x1000);
	if (l2 < 0) {
		log(LOG_WARNING, "out of mem for l2");
		exit(1);
	}

	log(LOG_INFO, "got code mem cid 0x%x", code_new);
	log(LOG_INFO, "got stack mem cid 0x%x", stack);
	log(LOG_INFO, "got l1 mem cid 0x%x", l1);
	log(LOG_INFO, "got l2 mem cid 0x%x", l2);

	log(LOG_INFO, "map code for copy");

	code_va = frame_map_anywhere(code_new);
	if (code_va == nil) {
		log(LOG_WARNING, "map failed");
		exit(1);
	}

	log(LOG_INFO, "copy code");

	memcpy(code_va, code, len);

	log(LOG_INFO, "unmap code");

	unmap_addr(code_new, code_va);
	
	log(LOG_INFO, "setup l1");

	if (frame_l1_setup(l1) != OK) {
		log(LOG_WARNING, "frame setup l1 failed");
		exit(1);
	}

	log(LOG_INFO, "map l2");

	if (frame_l2_map(l1, l2, 0x0) != OK) {
		log(LOG_WARNING, "frame l2 map failed");
		exit(1);
	}

	log(LOG_INFO, "map code");

	if (frame_map(l1, code_new, (void *) USER_ADDR, true) != OK) {
		log(LOG_WARNING, "frame map failed");
		exit(1);
	}

	log(LOG_INFO, "map stack");

	if (frame_map(l1, stack, (void *) (USER_ADDR - 0x1000), true) != OK) {
		log(LOG_WARNING, "frame map failed");
		exit(1);
	}

	log(LOG_INFO, "connect endpoint");

	eid = kcap_alloc();
	if (endpoint_connect(main_eid, eid) != OK) {
		log(LOG_WARNING, "endpoint connect failed");
		exit(1);
	}

	log(LOG_INFO, "alloc proc");

	p_cid = kobj_alloc(OBJ_proc, 1);
	if (p_cid < 0) {
		log(LOG_WARNING, "proc kobj alloc failed");
		exit(1);
	}
	
	log(LOG_INFO, "alloc caplist");

	clist = kobj_alloc(OBJ_caplist, 1);
	if (clist < 0) {
		log(LOG_WARNING, "proc kobj clist alloc failed");
		exit(1);
	}

	log(LOG_INFO, "setup proc");

	int p_pid;

	p_pid = proc_setup(p_cid, l1, clist, eid);
	if (p_pid < 0) {
		log(LOG_WARNING, "proc setup failed");
		exit(1);
	}

	kcap_free(l1);
	kcap_free(clist);
	kcap_free(eid);

	if (proc_set_priority(p_cid, priority) != OK) {
		log(LOG_WARNING, "proc set priority failed");
		exit(1);
	}

	log(LOG_INFO, "start bundled proc %s", name);

	if (proc_start(p_cid, USER_ADDR, USER_ADDR) < 0) {
		log(LOG_INFO, "proc start failed");
		exit(10);
	}

	return p_pid;
}

void
init_services(void)
{
	struct service *ser;
	int s;

	log(LOG_INFO, "setup services");

	for (s = 0; s < nservices; s++) {
		ser = &services[s];

		log(LOG_WARNING, "setup service %s", ser->name);

		ser->listen_eid = kobj_alloc(OBJ_endpoint, 1);
		if (ser->listen_eid < 0) {
			log(LOG_WARNING, "failed to create listen endpoint");
			exit(1);
		}

		log(LOG_INFO, "service %s listen endpoint %i", 
			ser->name, ser->listen_eid);
		
		ser->connect_eid = kcap_alloc();
		if (endpoint_connect(ser->listen_eid, ser->connect_eid) != OK) {
			log(LOG_WARNING, "failed to connect to listen endpoint");
			exit(1);
		}

		log(LOG_INFO, "service %s connect endpoint %i", 
			ser->name, ser->connect_eid);
	
		if (ser->device.is_device) {
			if (ser->device.reg != 0) {
				log(LOG_WARNING, "service %s create reg frame 0x%x 0x%x", 
					ser->name, ser->device.reg, ser->device.len);

				size_t pa, len;

				pa = PAGE_ALIGN_DN(ser->device.reg);
				len = PAGE_ALIGN(ser->device.len);

				ser->device.reg_cid = create_frame(pa, len, FRAME_DEV);

				if (ser->device.reg_cid < 0) {
					log(LOG_WARNING, "failed to create reg frame");
					exit(1);
				}
			} else {
				ser->device.reg_cid = -1;
			}

			if (ser->device.irqn > 0) {
				log(LOG_INFO, "service %s create int %i", 
					ser->name, ser->device.irqn);
				int cid;

				cid = kobj_alloc(OBJ_intr, 1);
				if (cid < 0) {
					log(LOG_WARNING, "failed to create obj for intr");
					exit(1);

				} else if (intr_init(cid, ser->device.irqn) != OK) {
					log(LOG_WARNING, "intr init for %i irqn %i failed", 
						cid, ser->device.irqn);
					exit(1);
				}

				ser->device.irq_cid = cid;
			} else {
				ser->device.irq_cid = -1;
			}
		}
	}
}
	void
init_procs(void)
{
	int i, s, pri;
	struct service *ser;
	size_t off;

	int bundle_cid;
	uint8_t *bundle_va;

	init_services();

	bundle_cid = create_frame(info->bundle_pa, 
		info->bundle_len, 
		FRAME_MEM);

	if (bundle_cid < 0) {
		log(LOG_WARNING, "failed to create bundle frame %i", 
			bundle_cid);
		exit(1);
	}

	bundle_va = frame_map_anywhere(bundle_cid);
	if (bundle_va == nil) {
		log(LOG_WARNING, "bundle map failed");
		exit(1);
	}

	log(LOG_INFO, "setup idle");

	off = 0;

	for (i = 0; i < nbundled_idle; i++) {
		init_bundled_proc(bundle_va + off,
			bundled_idle[i].name, 0,
			bundled_idle[i].len);

		off += bundled_idle[i].len;
	}

	log(LOG_INFO, "setup procs");

	for (i = 0; i < nbundled_procs; i++) {
		for (s = 0; s < nservices; s++) {
			ser = &services[s];
			if (!strcmp(ser->bin, bundled_procs[i].name))
				continue;

			if (ser->pid > 0) 
				continue;

			pri = ser->device.is_device ? 2 : 1;

			ser->pid = init_bundled_proc(bundle_va + off,
				bundled_procs[i].name, pri,
				bundled_procs[i].len);
		}
		
		off += bundled_procs[i].len;
	}

	if (unmap_addr(bundle_cid, bundle_va) != OK) {
		log(LOG_WARNING, "failed to unmap bundle");
		exit(1);
	}

	/* TODO: release bundle? */
}

