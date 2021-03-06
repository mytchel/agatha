#include "head.h"
#include "fns.h"
#include "trap.h"

void
init_kernel_drivers();

static uint8_t root_proc_obj[0x1000] = { 0 };
static uint8_t root_caplist_obj[0x1000] = { 0 };
static uint8_t root_untyped_obj[0x1000] = { 0 };
static obj_frame_t root_l1 = { 0 };

#define root_info_va    0x06000
#define root_code_va    USER_ADDR

static size_t va_next = 0;
static size_t kernel_va_min;
static size_t kernel_va_max;

uint32_t *kernel_l1 = nil;

	static void
root_start(void)
{
	label_t u = {0};

	debug_info("root drop to user\n");

	u.psr = MODE_USR;
	u.sp = root_code_va;
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
		c->caps[i].perm = 0;
		c->caps[i].obj = nil;
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

	info->root.info_va = root_info_va;
	info->root.stack_va = 
		root_code_va - info->root.stack_len;

	debug_info("map root stack at 0x%x\n", info->root.stack_va);

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

	proc_set_priority(p, 1);

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

	debug_info("kernel map 0x%x 0x%x (cache=%i) -> 0x%x\n",
		pa, len, cache, va_next);

	off = pa - PAGE_ALIGN_DN(pa);
	pa = PAGE_ALIGN_DN(pa);
	len = PAGE_ALIGN(len);

	va = va_next;

	if (va + len > kernel_va_max) {
		panic("out of kernel mappings 0x%x > max 0x%x\n", 
			va, kernel_va_max);
	}

	if (L1X(va) != L1X(va+len)) {
		va = L1X(va+len) << 20;
		if (L1X(va) != L1X(va+len)) {
			panic("trying to map 0x%x bytes which is too large\n",
				len);
		}
	}
	
	va_next = va + len;

	size_t o = L1X(va) - L1X(kernel_va_min);
	debug_info("0x%x is %i tables in\n", va, o);
	uint32_t *l2 = kernel_l2 + o * 1024;

	debug_info("kernel map l2 = 0x%x\n", l2);

	map_pages(l2, pa, va, len, AP_RW_NO, cache);

	return (void *) (va + off);
}

	void
kernel_unmap(void *addr, size_t len)
{
	size_t va = (size_t) addr;

	size_t o = L1X(va) - L1X(kernel_va_min);
	debug_info("0x%x is %i tables in\n", va, o);
	uint32_t *l2 = kernel_l2 + o * 1024;

	unmap_pages(l2, va, len);
}

	void
main(struct kernel_info *info)
{
	obj_proc_t *p;

	kernel_l1 = (uint32_t *) info->kernel.l1_va;
	kernel_l2 = (uint32_t *) info->kernel.l2_va;

	kernel_va_min = info->kernel_va;
	kernel_va_max = info->kernel_va + 
		((info->kernel.l2_len / TABLE_SIZE) * TABLE_AREA);

	va_next = info->kernel.l2_va + info->kernel.l2_len;

	unmap_l2(kernel_l1,
			TABLE_ALIGN_DN(info->boot_pa), 
			0x1000);

	vector_table_load();

	init_kernel_drivers();

	debug_info("kernel mapped at 0x%x\n", info->kernel_va);
	debug_info("kernel va min = 0x%x\n", kernel_va_min);
	debug_info("kernel va max = 0x%x\n", kernel_va_max);
	debug_info("info mapped at   0x%x\n", info);
	debug_info("boot   0x%x 0x%x\n", info->boot_pa, info->boot_len);
	debug_info("kernel 0x%x 0x%x\n", info->kernel_pa, info->kernel_len);
	debug_info("info   0x%x 0x%x\n", info->info_pa, info->info_len);
	debug_info("root  0x%x 0x%x\n", info->root_pa, info->root_len);
	debug_info("bundle 0x%x 0x%x\n", info->bundle_pa, info->bundle_len);
	debug_info("kernel l1 0x%x -> 0x%x 0x%x\n", 
			info->kernel.l1_pa, 
			info->kernel.l1_va, 
			info->kernel.l1_len);
	debug_info("kernel l2 0x%x -> 0x%x 0x%x\n", 
			info->kernel.l2_pa, 
			info->kernel.l2_va, 
			info->kernel.l2_len);
	debug_info("root l1 0x%x 0x%x\n", 
			info->root.l1_pa, 
			info->root.l1_len);
	debug_info("root l2 0x%x 0x%x\n", 
			info->root.l2_pa, 
			info->root.l2_len);
	debug_info("root stack 0x%x 0x%x\n", 
			info->root.stack_pa, 
			info->root.stack_len);

	p = init_root(info);

	kernel_unmap(info, info->info_len);

	debug_info("scheduling root\n");

	schedule(p);
}

