#include "head.h"
#include <arm/mmu.h>
#include "../bundle.h"
#include <log.h>

/* 

TODO: Most of this file needs improvements.
Frames should be objects that can contains a number
of address ranges and but can be treated as one piece? 

TODO: Frames should be able to be split.

TODO: L2 tables makes multiple proc0 mappings 
if they are mapped multiple times 

TODO: It is not very efficient and could be easily
improved.

TODO: L1 tables need to be made less
special so processes can map them 
in the same way they can map L2 tables.

 */

#if 0
struct proc procs[MAX_PROCS] = { 0 };

static uint8_t frame_pool_initial[sizeof(struct pool_frame)
	+ (sizeof(struct addr_frame) + sizeof(struct pool_obj)) * 1024];

static struct pool frame_pool;

	struct addr_frame *
frame_new(size_t pa, size_t len)
{
	struct addr_frame *n;

	n = pool_alloc(&frame_pool);
	if (n == nil) {
		return nil;
	}

	n->next = nil;
	n->pa = pa;
	n->len = len;
	n->table = false;
	n->mapped = 0;

	return n;
}

void
frame_free(struct addr_frame *f)
{
	release_addr(f->pa, f->len);

	pool_free(&frame_pool, f);
}

	int
proc_give_addr(int pid, struct addr_frame *f)
{
	f->table = false;
	f->mapped = 0;

	f->next = procs[pid].frames;
	procs[pid].frames = f;

	return OK;
}

	struct addr_frame *
proc_get_frame(int pid, size_t pa, size_t len)
{
	struct addr_frame *f;

	for (f = procs[pid].frames; f != nil; f = f->next) {
		if (f->pa <= pa && pa < f->pa + f->len) {
			if (f->pa + f->len < pa + len) {
				return nil;
			}

			return f;
		}
	}

	return nil;
}

struct addr_frame *
proc_take_addr(int pid, size_t pa, size_t len)
{
	struct addr_frame **f, *m;

	for (f = &procs[pid].frames; *f != nil; f = &(*f)->next) {
		if ((*f)->pa == pa && (*f)->len == len) {
			m = *f;

			if (m->mapped > 0 || m->table > 0) {
				return nil;
			}

			*f = (*f)->next;
			return m;
		}
	}

	return nil;
}

	static int
proc_map_table(int pid, 
		size_t pa, size_t va, 
		size_t len, int flags)
{
	struct addr_frame *f;
	uint32_t *addr;
	size_t o;

	f = proc_get_frame(pid, pa, len);
	if (f == nil) {
		return PROC0_ERR_PERMISSION_DENIED;
	}

	if (f->table == 0 && f->mapped > 0) {
		return PROC0_ERR_FLAG_INCOMPATABLE;
	}

	if (f->pa != pa || f->len != len) {
		/* TODO: should allow mappings to parts of frames */
		return ERR;
	}

	addr = map_addr(f->pa, f->len, MAP_RW|MAP_DEV);
	if (addr == nil) {
		return PROC0_ERR_INTERNAL;
	}

	if (f->table == 0) {
		memset(addr, 0, f->len);
	}

	for (o = 0; (o << 10) < f->len; o++) {
		f->table++;

		if (procs[pid].l1.table[L1X(va) + o] != L1_FAULT) {
			return PROC0_ERR_ADDR_DENIED;
		}

		procs[pid].l1.table[L1X(va) + o]
			= (pa + (o << 10)) | L1_COARSE;

		procs[pid].l1.mapped[L1X(va) + o]
			= ((uint32_t) addr) + (o << 10);
	}

	return OK;
}

	static int
proc_map_normal(int pid, 
		size_t pa, size_t va, 
		size_t len, int flags)
{
	struct addr_frame *f;
	uint32_t tex, c, b, o;
	uint32_t *l2_va;
	bool cache;
	int ap;

	f = proc_get_frame(pid, pa, len);
	if (f == nil) {
		return PROC0_ERR_PERMISSION_DENIED;
	}

	if (flags & MAP_RW) {
		ap = AP_RW_RW;
		if (f->table > 0) {
			return PROC0_ERR_FLAG_INCOMPATABLE;
		}
	} else {
		ap = AP_RW_RO;
	}

	if ((flags & MAP_TYPE_MASK) == MAP_MEM) {
		cache = true;
	} else if ((flags & MAP_TYPE_MASK) == MAP_DEV) {
		cache = false;
	} else {
		return PROC0_ERR_FLAGS;
	}

	if (cache) {
		tex = 7;
		c = 1;
		b = 0;
	} else {
		tex = 0;
		c = 0;
		b = 1;
	}

	for (o = 0; o < len; o += PAGE_SIZE) {
		l2_va = (void *) procs[pid].l1.mapped[L1X(va + o)];
		if (l2_va == nil) {
			return PROC0_ERR_TABLE;
		}

		if (l2_va[L2X(va + o)] != L2_FAULT) {
			return PROC0_ERR_ADDR_DENIED;
		}

		f->mapped++;

		l2_va[L2X(va + o)] = (pa + o)
			| L2_SMALL | tex << 6 | ap << 4 | c << 3 | b << 2;
	}

	return OK;
}

	int
proc_unmap_table(int pid,
		size_t va, size_t len)
{
	struct addr_frame *f;
	size_t o, pa;

	for (o = 0; (o << 10) < len; o++) {
		if (procs[pid].l1.mapped[L1X(va) + o] == nil)
			continue;

		pa = L1PA(procs[pid].l1.table[L1X(va) + o]);
		f = proc_get_frame(pid, pa, TABLE_SIZE);
		if (f == nil) {
			return PROC0_ERR_PERMISSION_DENIED;
		}

		f->table--;
		procs[pid].l1.mapped[L1X(va) + o] = nil;
	}

	return OK;
}

	int
proc_unmap_leaf(int pid, 
		size_t va, size_t len)
{
	struct addr_frame *f;
	uint32_t *l2_va;
	size_t o, pa;

	for (o = 0; o < len; o += PAGE_SIZE) {
		l2_va = (uint32_t *) procs[pid].l1.mapped[L1X(va + o)];
		if (l2_va == nil) {
			return PROC0_ERR_TABLE;
		}

		pa = L2PA(l2_va[L2X(va + o)]);

		f = proc_get_frame(pid, pa, PAGE_SIZE);
		if (f == nil) {
			return PROC0_ERR_PERMISSION_DENIED;
		}

		f->mapped--;
		l2_va[L2X(va + o)] = L2_FAULT;
	}

	return OK;
}

	int
proc_map(int pid, 
		size_t pa, size_t va, 
		size_t len, int flags)
{
	int type;

	/* Don't let procs map past kernel */
	if (info->kernel_va <= va + len) {
		log(LOG_INFO, "0x%x + 0x%x is beyond kernel 0x%x",
				va, len, info->kernel_va);
		return PROC0_ERR_ADDR_DENIED;
	}

	type = flags & MAP_TYPE_MASK;
	switch (type) {
		case MAP_REMOVE_TABLE:
			return proc_unmap_table(pid, va, len);

		case MAP_REMOVE_LEAF:
			return proc_unmap_leaf(pid, va, len);

		case MAP_TABLE:
			return proc_map_table(pid, pa, va, len, flags);

		case MAP_MEM:
		case MAP_DEV:
			return proc_map_normal(pid, pa, va, len, flags);

		default:
			return ERR;
	}
}
#endif

bool
init_bundled_proc(char *name,
		int priority,
		size_t start, size_t len,
		int *p_pid)
{
	int p_cid, eid, clist;
	int l1, l2;
	int code_bundle, code_new;
	int stack;
	void *bundle_va, *code_va;

	log(LOG_INFO, "init bundled %s", name);

	code_new = request_memory(len, 0x1000);
	if (code_new < 0) {
		log(LOG_WARNING, "out of mem");
		exit(1);
	}

	stack = request_memory(0x1000, 0x1000);
	if (stack < 0) {
		log(LOG_WARNING, "out of mem");
		exit(1);
	}

	code_bundle = kobj_alloc(OBJ_frame, 1);
	if (code_bundle < 0) {
		log(LOG_WARNING, "kobj alloc failed");
		exit(1);
	}

	if (frame_setup(code_bundle, FRAME_MEM, start, len) != OK) {
		log(LOG_WARNING, "frame setup failed");
		exit(1);
	}

	log(LOG_INFO, "map code to copy");

	bundle_va = (void *) 0x30000;
	code_va = (void *) ((size_t) bundle_va + len);

	if (frame_map(CID_L1, code_bundle, bundle_va) != OK) {
		log(LOG_WARNING, "map failed");
		exit(1);
	}

	if (frame_map(CID_L1, code_new, code_va) != OK) {
		log(LOG_WARNING, "map failed");
		exit(1);
	}

	log(LOG_INFO, "copy code");

	memcpy(code_va, bundle_va, len);

	log(LOG_INFO, "unmap code");

	frame_unmap(CID_L1, code_bundle, bundle_va, len);
	frame_unmap(CID_L1, code_new, code_va, len);

	kobj_free(code_bundle);
	
	log(LOG_INFO, "setup address space");

	l1 = request_memory(0x4000, 0x4000);
	if (l1 < 0) {
		log(LOG_WARNING, "out of mem");
		exit(1);
	}

	l2 = request_memory(0x1000, 0x1000);
	if (l2 < 0) {
		log(LOG_WARNING, "out of mem");
		exit(1);
	}

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

	if (frame_map(l1, code_new, (void *) USER_ADDR) != OK) {
		log(LOG_WARNING, "frame map failed");
		exit(1);
	}

	log(LOG_INFO, "map stack");

	if (frame_map(l1, stack, (void *) (USER_ADDR - 0x1000)) != OK) {
		log(LOG_WARNING, "frame map failed");
		exit(1);
	}

	log(LOG_INFO, "setup proc");

	eid = kcap_alloc();
	if (endpoint_connect(main_eid, eid) != OK) {
		log(LOG_WARNING, "endpoint connect failed");
		exit(1);
	}

	p_cid = kobj_alloc(OBJ_proc, 1);
	if (p_cid < 0) {
		log(LOG_WARNING, "proc kobj alloc failed");
		exit(1);
	}

	clist = kobj_alloc(OBJ_caplist, 1);
	if (clist < 0) {
		log(LOG_WARNING, "proc kobj clist alloc failed");
		exit(1);
	}

	if (proc_setup(p_cid, l1, clist, eid) != OK) {
		log(LOG_WARNING, "proc setup failed");
		exit(1);
	}

	if (proc_set_priority(p_cid, priority) != OK) {
		log(LOG_WARNING, "proc set priority failed");
		exit(1);
	}

	log(LOG_INFO, "start bundled proc pid %i %s", *p_pid, name);

	if (proc_start(p_cid, USER_ADDR, USER_ADDR) < 0) {
		log(LOG_INFO, "proc start failed");
		exit(10);
	}

	return true;
}

#if 0
bool
init_bundled_proc(char *name,
		int priority,
		size_t start, size_t len,
		int *p_pid)
{
	size_t code_pa, stack_pa;
	uint32_t *code_va, *start_va;
	
	size_t l1_table_pa, l1_mapped_pa;
	uint32_t *l1_table_va, *l1_mapped_va;

	size_t l2_pa;

	struct addr_frame *f;
	int r;

	log(LOG_INFO, "init bundled proc %s", name);

	code_pa = get_ram(len, 0x1000);
	if (code_pa == nil) {
		log(LOG_INFO, "out of mem");
		exit(1);
	}

	code_va = map_addr(code_pa, len, MAP_RW|MAP_DEV);
	if (code_va == nil) {
		log(LOG_INFO, "map failed");
		exit(1);
	}

	start_va = map_addr(start, len, MAP_RO|MAP_DEV);
	if (start_va == nil) {
		log(LOG_INFO, "map failed");
		exit(1);
	}

	memcpy(code_va, start_va, len);

	if ((r = unmap_addr(code_va, len)) != OK) {
		log(LOG_INFO, "unmap failed");
		exit(r);
	}

	if ((r = unmap_addr(start_va, len)) != OK) {
		log(LOG_INFO, "unmap failed");
		exit(r);
	}
	
	l1_table_pa = get_ram(0x4000, 0x4000);
	if (l1_table_pa == nil) {
		log(LOG_INFO, "out of mem");
		exit(1);
	}

	l1_mapped_pa = get_ram(0x4000, 0x1000);
	if (l1_mapped_pa == nil) {
		log(LOG_INFO, "out of mem");
		exit(1);
	}

	l1_table_va = map_addr(l1_table_pa, 0x4000, MAP_RW|MAP_DEV);
	if (l1_table_va == nil) {
		log(LOG_INFO, "map failed");
		exit(1);
	}

	l1_mapped_va = map_addr(l1_mapped_pa, 0x4000, MAP_RW|MAP_DEV);
	if (l1_mapped_va == nil) {
		log(LOG_INFO, "map failed");
		exit(1);
	}

	memset(l1_mapped_va, 0, 0x4000);
	
	proc_init_l1(l1_table_va);

	int eid, p_cid;

	eid = kcap_alloc();
	if (endpoint_connect(main_eid, eid) != OK) {
		exit(1);
	}

	p_cid = kobj_alloc(OBJ_proc, 1);
	if (p_cid < 0) {
		exit(1);
	}

	*p_pid = proc_setup(p_cid, l1_table_pa, priority, eid);
	if (*p_pid < 0) {
		log(LOG_INFO, "proc setup failed");
		exit(1);
	}

	procs[*p_pid].l1.table = l1_table_va;
	procs[*p_pid].l1.mapped = l1_mapped_va;
	procs[*p_pid].l1.table_pa = l1_table_pa;
	procs[*p_pid].l1.mapped_pa = l1_mapped_pa;

	/* Initial l2 */

	l2_pa = get_ram(0x1000, 0x1000);
	if (l2_pa == nil) {
		log(LOG_INFO, "out of mem");
		exit(1);
	}

	if ((f = frame_new(l2_pa, 0x1000)) == nil) {
		log(LOG_INFO, "out of mem");
		exit(1);
	}

	if ((r = proc_give_addr(*p_pid, f)) != OK) {
		log(LOG_INFO, "give addr failed");
		exit(1);
	}

	if ((r = proc_map(*p_pid, l2_pa, 
				0, 0x1000, 
				MAP_TABLE)) != OK) 
	{
		log(LOG_INFO, "proc map failed");
		exit(r);
	}

	/* Stack */

	size_t stack_len = 0x2000;
	stack_pa = get_ram(stack_len, 0x1000); 
	if (stack_pa == nil) {
		log(LOG_INFO, "out of mem");
		exit(1);
	}

	if ((f = frame_new(stack_pa, stack_len)) == nil) {
		log(LOG_INFO, "out of mem");
		exit(0xb);
	}

	if ((r = proc_give_addr(*p_pid, f)) != OK) {
		log(LOG_INFO, "give addr failed");
		exit(0xc);
	}

	if ((r = proc_map(*p_pid, stack_pa, 
				USER_ADDR - stack_len, stack_len, 
				MAP_MEM|MAP_RW)) != OK) 
	{
		log(LOG_INFO, "proc map failed");
		exit(0x10);
	}

	/* Code */

	if ((f = frame_new(code_pa, len)) == nil) {
		exit(0xd);
	}

	if ((r = proc_give_addr(*p_pid, f)) != OK) {
		exit(0xe);
	}

	if ((r = proc_map(*p_pid, code_pa, 
				USER_ADDR, len, 
				MAP_MEM|MAP_RW)) != OK) 
	{
		exit(0x11);
	}

	log(LOG_INFO, "start bundled proc pid %i %s", *p_pid, name);

	if (proc_start(p_cid, USER_ADDR, USER_ADDR) < 0) {
		log(LOG_INFO, "proc start failed");
		exit(10);
	}

	return true;
}

#endif


	void
init_procs(void)
{
	int i, s, pri, p_pid;
	struct service *ser;
	size_t off;
#if 0
	if (pool_init(&frame_pool, sizeof(struct addr_frame)) != OK) {
		exit(1);
	}

	if (pool_load(&frame_pool, 
			frame_pool_initial, 
			sizeof(frame_pool_initial)) != OK) 
	{
		exit(1);
	}
#endif

#if 0
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

				ser->device.reg_frame = 
					frame_new(PAGE_ALIGN_DN(ser->device.reg),
							PAGE_ALIGN(ser->device.len));

				if (ser->device.reg_frame == nil) {
					log(LOG_WARNING, "failed to create reg frame");
					exit(1);
				}
			} else {
				ser->device.reg_frame = nil;
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
#endif

	log(LOG_INFO, "setup idle");

	off = info->bundle_pa;

	for (i = 0; i < nbundled_idle; i++) {
		init_bundled_proc(bundled_idle[i].name, 0,
				off, bundled_idle[i].len,
				&p_pid);

		off += bundled_idle[i].len;
	}

#if 0	
	log(LOG_INFO, "setup procs");

	for (i = 0; i < nbundled_procs; i++) {
		for (s = 0; s < nservices; s++) {
			ser = &services[s];
			if (!strcmp(ser->bin, bundled_procs[i].name))
				continue;

			if (ser->pid > 0) 
				continue;

			pri = ser->device.is_device ? 2 : 1;

			init_bundled_proc(bundled_procs[i].name, pri,
					off, bundled_procs[i].len,
					&ser->pid);
		}
		
		off += bundled_procs[i].len;
	}

#endif

}


