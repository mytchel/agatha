#include "head.h"
#include "fns.h"
#include "trap.h"

void
init_kernel_drivers();

static uint8_t root_proc_obj[0x1000] = { 0 };
static uint8_t root_caplist_obj[0x1000] = { 0 };
static uint8_t root_untyped_obj[0x1000] = { 0 };
static obj_frame_t root_l1 = { 0 };

#define root_l1_va      0x01000
#define root_l2_va      0x05000
#define root_info_va    0x06000
#define root_stack_va   0x1c000
#define root_code_va    USER_ADDR

static size_t va_next = 0;
static size_t max_kernel_va;

uint32_t *kernel_l1 = nil;

	static void
root_start(void)
{
	label_t u = {0};

	debug_info("root drop to user\n");

	u.psr = MODE_USR;
	u.sp = root_stack_va + 0x1000;
	u.pc = root_code_va;
	u.regs[0] = root_info_va;

	drop_to_user(&u);
}

	static obj_proc_t *
init_root(struct kernel_info *info)
{
	uint32_t *l1, *l2;
	obj_proc_t *p;
	obj_caplist_t *c;

	p = (void *) root_proc_obj;
	p->h.type = OBJ_proc;
	p->h.refs = 1;

	int i;

	c = (void *) root_caplist_obj;
	c->h.type = OBJ_caplist;
	c->h.refs = 1;
	for (i = 0; i < 255; i++) {
		c->caps[i].id = i;
		c->caps[i].perm = 0;
		c->caps[i].obj = nil;

		if (i < 255)
			c->caps[i].next = &c->caps[i+1];
		else
			c->caps[i].next = nil;
	}

	l1 = kernel_map(info->root.l1_pa,
			info->root.l1_len,
			false);

	l2 = kernel_map(info->root.l2_pa,
			info->root.l2_len,
			false);

	memcpy(l1,
			kernel_l1,
			0x4000);

	map_l2(l1, info->root.l2_pa,
			0, 0x1000);

	info->root.l1_va = root_l1_va;
	info->root.l2_va = root_l2_va;
	info->root.info_va = root_info_va;
	info->root.stack_va = root_stack_va;

	map_pages(l2, 
			info->root.l1_pa, 
			info->root.l1_va, 
			info->root.l1_len,
			AP_RW_RW, false);

	map_pages(l2, 
			info->root.l2_pa, 
			info->root.l2_va, 
			info->root.l2_len,
			AP_RW_RW, false);

	map_pages(l2, 
			info->info_pa,
			root_info_va,
			info->info_len,
			AP_RW_RW, true);

	map_pages(l2, 
			info->root.stack_pa,
			info->root.stack_va,
			info->root.stack_len, 
			AP_RW_RW, true);

	map_pages(l2, 
			info->root_pa,
			root_code_va,
			info->root_len, 
			AP_RW_RW, true);

	kernel_unmap(l1, info->root.l1_len);
	kernel_unmap(l2, info->root.l2_len);

	if (proc_init(p) != OK) {
		panic("Failed to init root!\n");
	}

	if (p->pid != ROOT_PID) {
		panic("root proc (%i) doesn't have root pid (%i)!\n",
			p->pid, ROOT_PID);
	}

	p->vspace = info->root.l1_pa;
	p->cap_root = c;

	root_l1.h.refs = 1;
	root_l1.h.type = OBJ_frame;
	root_l1.pa = info->root.l1_pa;
	root_l1.len = info->root.l1_len;
	root_l1.type = FRAME_L1;

	c->caps[CID_CLIST >> 12].obj = (void *) c;
	c->caps[CID_CLIST >> 12].perm = CAP_write | CAP_read;

	c->caps[CID_L1 >> 12].obj = (void *) &root_l1;
	c->caps[CID_L1 >> 12].perm = CAP_write | CAP_read;

	debug_info("root clist 0x%x\n", c);
	debug_info("root l1 0x%x\n", &root_l1);

	obj_untyped_t *root_initial_obj = (void *) root_untyped_obj;
	root_initial_obj->h.type = OBJ_untyped;
	root_initial_obj->h.refs = 1;
	root_initial_obj->len = sizeof(root_untyped_obj);

	c->caps[(CID_L1>>12)+1].obj = (void *) root_initial_obj;
	c->caps[(CID_L1>>12)+1].perm = CAP_write | CAP_read;

	func_label(&p->label, 
			(size_t) p->kstack, KSTACK_LEN, 
			(size_t) &root_start,
			0, 0);

	proc_ready(p);

	return p;
}

static uint32_t *kernel_l2;

	void *
kernel_map(size_t pa, size_t len, bool cache)
{
	size_t va, off;

	off = pa - PAGE_ALIGN_DN(pa);
	pa = PAGE_ALIGN_DN(pa);
	len = PAGE_ALIGN(len);

	va = va_next;
	va_next += len;

	if (va > max_kernel_va) {
		panic("out of kernel mappings 0x%x > max 0x%x\n", va, max_kernel_va);
	}

	map_pages(kernel_l2, pa, va, len, AP_RW_NO, cache);

	return (void *) (va + off);
}

	void
kernel_unmap(void *addr, size_t len)
{
	unmap_pages(kernel_l2, (size_t) addr, len);
}

	void
main(struct kernel_info *info)
{
	obj_proc_t *p;

	kernel_l1 = (uint32_t *) info->kernel.l1_va;
	kernel_l2 = (uint32_t *) info->kernel.l2_va;

	max_kernel_va = info->kernel_va + 
		((info->kernel.l2_len / TABLE_SIZE) * TABLE_AREA);

	va_next = info->kernel.l2_va + info->kernel.l2_len;

	unmap_l2(kernel_l1,
			TABLE_ALIGN_DN(info->boot_pa), 
			0x1000);

	vector_table_load();

	init_kernel_drivers();

	debug(DEBUG_INFO, "kernel mapped at 0x%x\n", info->kernel_va);
	debug(DEBUG_INFO, "max kernel va = 0x%x\n", max_kernel_va);
	debug(DEBUG_INFO, "info mapped at   0x%x\n", info);
	debug(DEBUG_INFO, "boot   0x%x 0x%x\n", info->boot_pa, info->boot_len);
	debug(DEBUG_INFO, "kernel 0x%x 0x%x\n", info->kernel_pa, info->kernel_len);
	debug(DEBUG_INFO, "info   0x%x 0x%x\n", info->info_pa, info->info_len);
	debug(DEBUG_INFO, "root  0x%x 0x%x\n", info->root_pa, info->root_len);
	debug(DEBUG_INFO, "bundle 0x%x 0x%x\n", info->bundle_pa, info->bundle_len);
	debug(DEBUG_INFO, "kernel l1 0x%x -> 0x%x 0x%x\n", 
			info->kernel.l1_pa, 
			info->kernel.l1_va, 
			info->kernel.l1_len);
	debug(DEBUG_INFO, "kernel l2 0x%x -> 0x%x 0x%x\n", 
			info->kernel.l2_pa, 
			info->kernel.l2_va, 
			info->kernel.l2_len);
	debug(DEBUG_INFO, "root l1 0x%x 0x%x\n", 
			info->root.l1_pa, 
			info->root.l1_len);
	debug(DEBUG_INFO, "root l2 0x%x 0x%x\n", 
			info->root.l2_pa, 
			info->root.l2_len);

	p = init_root(info);

	kernel_unmap(info, info->info_len);

	debug_info("scheduling root\n");

	schedule(p);
}

