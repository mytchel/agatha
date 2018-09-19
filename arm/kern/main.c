#include "../../port/head.h"
#include "fns.h"
#include "trap.h"

void
get_intc();

void
get_systick();

void
get_serial();

uint32_t
kernel_info[1024] __attribute__((__aligned__(0x1000))) = { 0 };

uint32_t
kernel_l1[4096]__attribute__((__aligned__(0x4000))) = { 0 };

uint32_t
kernel_l2[1024]__attribute__((__aligned__(0x1000))) = { 0 };

static uint32_t
proc0_l1[4096]__attribute__((__aligned__(0x4000)));

static uint32_t
proc0_l2[1024]__attribute__((__aligned__(0x1000))) = { 0 };

static uint32_t
proc0_stack[1024*4]__attribute__((__aligned__(0x1000))) = { 0 };

extern uint32_t *_binary_proc0_bin_start;
extern uint32_t *_binary_proc0_bin_end;
extern uint32_t *_binary_bundle_bin_start;
extern uint32_t *_binary_bundle_bin_end;

extern uint32_t *_kernel_start;
extern uint32_t *_kernel_end;

static struct kernel_info *info = (struct kernel_info *) kernel_info;

	static void
proc0_start(void)
{
	label_t u = {0};

	u.psr = MODE_USR;
	u.sp = info->proc0.stack_va 
		+ info->proc0.stack_len;

	u.pc = info->proc0.prog_va;
	u.regs[0] = info->proc0.info_va;

	drop_to_user(&u, up->kstack, KSTACK_LEN);
}

	static proc_t
init_proc0(void)
{
	proc_t p;

	p = proc_new();
	if (p == nil) {
		panic("Failed to create proc0 entry!\n");
	}

	func_label(&p->label, (size_t) p->kstack, KSTACK_LEN, 
			(size_t) &proc0_start);

	memcpy(proc0_l1,
			kernel_l1, 
			0x4000);

	map_l2(proc0_l1, (size_t) proc0_l2,
			0, 0x1000);

	info->proc0.l1_pa = (size_t) proc0_l1;
	info->proc0.l1_va = (uint32_t *) 0x1000;
	info->proc0.l1_len = 0x4000;

	map_pages(proc0_l2, 
			info->proc0.l1_pa, 
			(size_t) info->proc0.l1_va, 
			info->proc0.l1_len,
			AP_RW_RW, true);

	info->proc0.l2_pa = (size_t) proc0_l2;
	info->proc0.l2_va = (uint32_t *) 0x5000;
	info->proc0.l2_len = 0x1000;

	map_pages(proc0_l2, 
			info->proc0.l2_pa, 
			(size_t) info->proc0.l2_va, 
			info->proc0.l2_len,
			AP_RW_RW, true);

	info->proc0.info_va = 0x6000;

	map_pages(proc0_l2, 
			info->info_pa,
			info->proc0.info_va, 
			info->info_len,
			AP_RW_RW, true);

	info->proc0.prog_pa = (size_t) &_binary_proc0_bin_start;
	info->proc0.prog_va = USER_ADDR;
	info->proc0.prog_len = 
		PAGE_ALIGN(&_binary_proc0_bin_end) -
		(size_t) &_binary_proc0_bin_start;

	map_pages(proc0_l2, 
			info->proc0.prog_pa,
			info->proc0.prog_va, 
			info->proc0.prog_len, 
			AP_RW_RW, true);

	info->proc0.stack_pa = (size_t) proc0_stack;
	info->proc0.stack_va = USER_ADDR - sizeof(proc0_stack);
	info->proc0.stack_len = sizeof(proc0_stack);

	map_pages(proc0_l2, 
			info->proc0.stack_pa,
			info->proc0.stack_va,
			info->proc0.stack_len, 
			AP_RW_RW, true);

	p->vspace = info->proc0.l1_pa;

	return p;
}

	void *
kernel_map(size_t pa, size_t len, int ap, bool cache)
{
	static size_t va_next = 0;
	size_t va;

	if (va_next == 0) {
		va_next = info->kernel_pa + info->kernel_len;
	}

	va = va_next;

	va_next += PAGE_ALIGN(len);

	map_pages(kernel_l2, pa, va, len, ap, cache);

	return (void *) va;
}

	void
main(void)
{
	proc_t p0;

	info->kernel_pa    = (size_t) &_kernel_start;
	info->kernel_len   = 
		PAGE_ALIGN(&_kernel_end) - 
		(size_t) &_kernel_start;

	info->bundle_pa = (size_t) &_binary_bundle_bin_start;
	info->bundle_len   = 
		PAGE_ALIGN((size_t) &_binary_bundle_bin_end) - 
		(size_t) &_binary_bundle_bin_start;

	info->info_pa = (size_t) kernel_info;
	info->info_len = sizeof(kernel_info);

	map_l2(kernel_l1, (size_t) kernel_l2, 
			info->kernel_pa, 0x1000);

	map_pages(kernel_l2, 
			info->kernel_pa, 
			info->kernel_pa, 
			info->kernel_len, 
			AP_RW_NO, true);

	mmu_load_ttb(kernel_l1);
	mmu_invalidate();
	mmu_enable();

	get_serial();
	get_intc();
	get_systick();

	p0 = init_proc0();

	debug("scheduling proc0\n");

	schedule(p0);
}

