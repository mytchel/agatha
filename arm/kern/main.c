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

static struct kernel_info *info;
static size_t va_next = 0;

	static void
proc0_start(void)
{
	label_t u = {0};

	u.psr = MODE_USR;
	u.sp = info->proc0.stack_va + info->proc0.stack_len;
	u.pc = proc0_code_va;
	u.regs[0] = proc0_info_va;

	drop_to_user(&u);
}

	static proc_t
init_proc0(void)
{
	uint32_t *l1, *l2;
	proc_t p;

	l1 = kernel_map(info->proc0.l1_pa,
			info->proc0.l1_len,
			AP_RW_NO,
			true);

	l2 = kernel_map(info->proc0.l2_pa,
			info->proc0.l2_len,
			AP_RW_NO,
			true);

	memcpy(l1,
			info->kernel.l1_va, 
			0x4000);

	map_l2(l1, info->proc0.l2_pa,
			0, 0x1000);

	info->proc0.l1_va = (uint32_t *) proc0_l1_va;
	info->proc0.l2_va = (uint32_t *) proc0_l2_va;
	info->proc0.info_va = proc0_info_va;
	info->proc0.stack_va = proc0_stack_va;
	info->proc0.prog_va = USER_ADDR;

	map_pages(l2, 
			info->proc0.l1_pa, 
			(size_t) info->proc0.l1_va, 
			info->proc0.l1_len,
			AP_RW_RW, true);

	map_pages(l2, 
			info->proc0.l2_pa, 
			(size_t) info->proc0.l2_va, 
			info->proc0.l2_len,
			AP_RW_RW, true);

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
			info->proc0.prog_pa,
			info->proc0.prog_va, 
			info->proc0.prog_len, 
			AP_RW_RW, true);

	kernel_unmap(l1, info->proc0.l1_len);
	kernel_unmap(l2, info->proc0.l2_len);

	p = proc_new(info->proc0.l1_pa, 0);
	if (p == nil) {
		panic("Failed to create proc0 entry!\n");
	}

	func_label(&p->label, (size_t) p->kstack, KSTACK_LEN, 
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

	map_pages(info->kernel.l2_va, pa, va, len, ap, cache);

	return (void *) (va + off);
}

	void
kernel_unmap(void *addr, size_t len)
{
	unmap_pages(info->kernel.l2_va, (size_t) addr, len);
}

	void
main(struct kernel_info *i)
{
	proc_t p0;

	info = i;
	va_next = ((size_t) info->kernel.l2_va)
	 	+ info->kernel.l2_len;

	unmap_l2(info->kernel.l1_va,
			TABLE_ALIGN(info->boot_pa), 
			0x1000);

	vector_table_load();

	init_kernel_drivers();

	p0 = init_proc0();

	debug(DEBUG_SCHED, "scheduling proc0\n");

	schedule(p0);
}

