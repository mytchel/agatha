#include "../../sys/head.h"
#include "fns.h"
#include "trap.h"

void
init_kernel_drivers();

#define root_l1_va      0x01000
#define root_l2_va      0x05000
#define root_info_va    0x06000
#define root_stack_va   0x1c000
#define root_code_va    USER_ADDR

static size_t va_next = 0;

static uint32_t *kernel_l1;
static uint32_t *kernel_l2;

	static void
root_start(void)
{
	label_t u = {0};

	u.psr = MODE_USR;
	u.sp = root_stack_va + 0x1000;
	u.pc = root_code_va;
	u.regs[0] = root_info_va;

	drop_to_user(&u);
}

	static proc_t *
init_root(struct kernel_info *info)
{
	uint32_t *l1, *l2;
	proc_t *p;

	l1 = kernel_map(info->root.l1_pa,
			info->root.l1_len,
			AP_RW_RW,
			false);

	l2 = kernel_map(info->root.l2_pa,
			info->root.l2_len,
			AP_RW_RW,
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

	p = proc_new(1, info->root.l1_pa);
	if (p == nil) {
		panic("Failed to create root!\n");
	}

	if (p->pid != ROOT_PID) {
		panic("root proc (%i) doesn't have root pid (%i)!\n",
			p->pid, ROOT_PID);
	}

	debug_info("func label %i, stack top at 0x%x\n",
			p->pid, p->kstack + KSTACK_LEN);

	func_label(&p->label, 
			(size_t) p->kstack, KSTACK_LEN, 
			(size_t) &root_start);

	proc_ready(p);

	return p;
}

/* TODO: these don't do much */

	void *
kernel_map(size_t pa, size_t len, int ap, bool cache)
{
	size_t va, off;

	off = pa - PAGE_ALIGN_DN(pa);
	pa = PAGE_ALIGN_DN(pa);
	len = PAGE_ALIGN(len);

	va = va_next;
	va_next += len;

	map_pages(kernel_l2, pa, va, len, ap, cache);

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
	proc_t *p1;

	kernel_l1 = (uint32_t *) info->kernel.l1_va;
	kernel_l2 = (uint32_t *) info->kernel.l2_va;

	va_next = info->kernel.l2_va + info->kernel.l2_len;

	unmap_l2(kernel_l1,
			TABLE_ALIGN_DN(info->boot_pa), 
			0x1000);

	vector_table_load();

	init_kernel_drivers();

	debug(DEBUG_INFO, "kernel mapped at 0x%x\n", info->kernel_va);
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

	p1 = init_root(info);

	kernel_unmap(info, info->info_len);

	debug(DEBUG_SCHED, "scheduling root\n");

	schedule(p1);
}

