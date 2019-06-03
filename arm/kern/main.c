#include "../../sys/head.h"
#include "fns.h"
#include "trap.h"

void
init_kernel_drivers();

#define proc0_l1_va      0x01000
#define proc0_l2_va      0x05000
#define proc0_info_va    0x06000
#define proc0_stack_va   0x1c000
#define proc0_code_va    USER_ADDR

static size_t va_next = 0;

static uint32_t *kernel_l1;
static uint32_t *kernel_l2;

	static void
proc0_start(void)
{
	label_t u = {0};

	u.psr = MODE_USR;
	u.sp = proc0_stack_va + 0x1000;
	u.pc = proc0_code_va;
	u.regs[0] = proc0_info_va;

	drop_to_user(&u);
}

	static proc_t
init_proc0(struct kernel_info *info)
{
	uint32_t *l1, *l2;
	proc_t p;

	l1 = kernel_map(info->proc0.l1_pa,
			info->proc0.l1_len,
			AP_RW_RW,
			false);

	l2 = kernel_map(info->proc0.l2_pa,
			info->proc0.l2_len,
			AP_RW_RW,
			false);

	memcpy(l1,
			kernel_l1,
			0x4000);

	map_l2(l1, info->proc0.l2_pa,
			0, 0x1000);

	info->proc0.l1_va = proc0_l1_va;
	info->proc0.l2_va = proc0_l2_va;
	info->proc0.info_va = proc0_info_va;
	info->proc0.stack_va = proc0_stack_va;

	map_pages(l2, 
			info->proc0.l1_pa, 
			info->proc0.l1_va, 
			info->proc0.l1_len,
			AP_RW_RW, false);

	map_pages(l2, 
			info->proc0.l2_pa, 
			info->proc0.l2_va, 
			info->proc0.l2_len,
			AP_RW_RW, false);

	map_pages(l2, 
			info->info_pa,
			proc0_info_va,
			info->info_len,
			AP_RW_RW, true);

	map_pages(l2, 
			info->proc0.stack_pa,
			info->proc0.stack_va,
			info->proc0.stack_len, 
			AP_RW_RW, true);

	map_pages(l2, 
			info->proc0_pa,
			proc0_code_va,
			info->proc0_len, 
			AP_RW_RW, true);

	kernel_unmap(l1, info->proc0.l1_len);
	kernel_unmap(l2, info->proc0.l2_len);

	p = proc_new(info->proc0.l1_pa, 0, 1);
	if (p == nil) {
		panic("Failed to create proc0 entry!\n");
	}

	debug_info("func label %i, stack top at 0x%x\n",
			p->pid, p->kstack + KSTACK_LEN);

	func_label(&p->label, 
			(size_t) p->kstack, KSTACK_LEN, 
			(size_t) &proc0_start);

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
	proc_t p0;

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
	debug(DEBUG_INFO, "proc0  0x%x 0x%x\n", info->proc0_pa, info->proc0_len);
	debug(DEBUG_INFO, "bundle 0x%x 0x%x\n", info->bundle_pa, info->bundle_len);
	debug(DEBUG_INFO, "kernel l1 0x%x -> 0x%x 0x%x\n", 
			info->kernel.l1_pa, 
			info->kernel.l1_va, 
			info->kernel.l1_len);
	debug(DEBUG_INFO, "kernel l2 0x%x -> 0x%x 0x%x\n", 
			info->kernel.l2_pa, 
			info->kernel.l2_va, 
			info->kernel.l2_len);
	debug(DEBUG_INFO, "proc0 l1 0x%x 0x%x\n", 
			info->proc0.l1_pa, 
			info->proc0.l1_len);
	debug(DEBUG_INFO, "proc0 l2 0x%x 0x%x\n", 
			info->proc0.l2_pa, 
			info->proc0.l2_len);

	p0 = init_proc0(info);

	kernel_unmap(info, info->info_len);

	debug(DEBUG_SCHED, "scheduling proc0\n");

	schedule(p0);
}

