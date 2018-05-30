#include "../../port/head.h"
#include "fns.h"
#include "trap.h"

struct kernel_info
kernel_info __attribute__((__aligned__(0x1000))) = { 0 };

uint32_t
kernel_ttb[4096]__attribute__((__aligned__(0x4000))) = { 0 };

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

size_t kernel_va_slot;

	static void
proc0_start(void)
{
	label_t u = {0};

	u.psr = MODE_USR;
	u.sp = kernel_info.stack_va 
		+ kernel_info.stack_len;

	u.pc = kernel_info.prog_va;
	u.regs[0] = kernel_info.kernel_info_va;

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
			kernel_ttb, 
			0x4000);

	map_l2(proc0_l1, (size_t) proc0_l2, 0);

	kernel_info.l1_pa       = (size_t) proc0_l1;
	kernel_info.l1_va       = (uint32_t *) 0x1000;
	kernel_info.l1_len      = 0x4000;

	map_pages(proc0_l2, 
			kernel_info.l1_pa, 
			(size_t) kernel_info.l1_va, 
			kernel_info.l1_len,
			AP_RW_RW, true);

	kernel_info.l2_pa       = (size_t) proc0_l2;
	kernel_info.l2_va       = (uint32_t *) 0x5000;
	kernel_info.l2_len      = 0x1000;

	map_pages(proc0_l2, 
			kernel_info.l2_pa, 
			(size_t) kernel_info.l2_va, 
			kernel_info.l2_len,
			AP_RW_RW, true);

	kernel_info.kernel_info_pa       = 
		(size_t) &kernel_info;
	kernel_info.kernel_info_va       = 
		(size_t) kernel_info.l2_va + sizeof(proc0_l2);
	kernel_info.kernel_info_len      = 
		PAGE_ALIGN(sizeof(kernel_info));

	map_pages(proc0_l2, 
			kernel_info.kernel_info_pa,
			kernel_info.kernel_info_va, 
			kernel_info.kernel_info_len,
			AP_RW_RW, true);

	kernel_info.prog_pa = (size_t) &_binary_proc0_bin_start;
	kernel_info.prog_va = USER_ADDR;
	kernel_info.prog_len = PAGE_ALIGN(&_binary_proc0_bin_end) 
		- (size_t) &_binary_proc0_bin_start;

	map_pages(proc0_l2, 
			kernel_info.prog_pa,
		 	kernel_info.prog_va, 
			kernel_info.prog_len, 
			AP_RW_RW, true);

	kernel_info.stack_pa = (size_t) proc0_stack;
	kernel_info.stack_va = USER_ADDR - sizeof(proc0_stack);
	kernel_info.stack_len = sizeof(proc0_stack);

	map_pages(proc0_l2, 
			kernel_info.stack_pa,
			kernel_info.stack_va,
		 	kernel_info.stack_len, 
			AP_RW_RW, true);

	p->vspace = (size_t) proc0_l1;

	return p;
}

	void
main(size_t kernel_start, size_t dtb_start, size_t dtb_len)
{
	size_t kernel_len;
	proc_t p0;

	kernel_len = PAGE_ALIGN(&_kernel_end) - kernel_start;

	kernel_info.kernel_start = kernel_start;
	kernel_info.kernel_len   = kernel_len;

	kernel_info.dtb_start    = dtb_start;
	kernel_info.dtb_len      = dtb_len;

	kernel_info.bundle_start = (size_t) &_binary_bundle_bin_start;
	kernel_info.bundle_len   = 
		(size_t) &_binary_bundle_bin_end - 
		(size_t) &_binary_bundle_bin_start;

	map_l2(kernel_ttb, (size_t) kernel_l2, kernel_start);

	kernel_va_slot = kernel_start;

	map_pages(kernel_l2, kernel_start, kernel_va_slot, 
			kernel_len, AP_RW_NO, true);

	kernel_va_slot += kernel_len;

	map_devs();

	mmu_load_ttb(kernel_ttb);
	mmu_invalidate();
	mmu_enable();

	init_devs();

	p0 = init_proc0();

	schedule(p0);
}

