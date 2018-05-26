#include "../../kern/head.h"
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
proc0_stack[4096]__attribute__((__aligned__(0x1000))) = { 0 };

extern uint32_t *_binary_proc0_bin_start;
extern uint32_t *_binary_proc0_bin_end;
extern uint32_t *_binary_bundle_bin_start;
extern uint32_t *_binary_bundle_bin_end;

extern uint32_t *_kernel_start;
extern uint32_t *_kernel_end;

size_t kernel_va_slot;
size_t proc0_kernel_info_va;

	static void
proc0_start(void)
{
	label_t u = {0};

	u.psr = MODE_USR;
	u.sp = USER_ADDR;
	u.pc = USER_ADDR;
	u.regs[0] = proc0_kernel_info_va;

	drop_to_user(&u, up->kstack, KSTACK_LEN);
}

	static proc_t
init_proc0(void)
{
	size_t s, l;
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

	map_pages(proc0_l2, (size_t) proc0_l1, 0x1000, 
			sizeof(proc0_l1),
			AP_RW_RW, true);

	map_pages(proc0_l2, (size_t) proc0_l2, 0x1000 + sizeof(proc0_l1),
			sizeof(proc0_l2), 
			AP_RW_RW, true);

	proc0_kernel_info_va = 0x1000 + sizeof(proc0_l1) + sizeof(proc0_l2);
	map_pages(proc0_l2, (size_t) &kernel_info, proc0_kernel_info_va,
			sizeof(kernel_info), 
			AP_RW_RW, true);

	s = (size_t) &_binary_proc0_bin_start;
	l = PAGE_ALIGN(&_binary_proc0_bin_end) - s;

	map_pages(proc0_l2, 
			s, 
			USER_ADDR,
			l, 
			AP_RW_RW, true);

	s = (size_t) proc0_stack;
	l = sizeof(proc0_stack);

	map_pages(proc0_l2, s, USER_ADDR - l,
			l, 
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
	kernel_info.bundle_start = (size_t) &_binary_bundle_bin_start;
	kernel_info.bundle_len   = 
	(size_t) &_binary_bundle_bin_end - (size_t) &_binary_bundle_bin_start;
	kernel_info.dtb_start    = dtb_start;
	kernel_info.dtb_len      = dtb_len;

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

	debug("kernel_start: 0x%h \n", kernel_start);
	debug("proc0_start : 0x%h \n", &_binary_proc0_bin_start);
	debug("proc0_end   : 0x%h \n", &_binary_proc0_bin_end);
	debug("bundle_start: 0x%h \n", &_binary_bundle_bin_start);
	debug("bundle_end  : 0x%h \n", &_binary_bundle_bin_end);

	p0 = init_proc0();

	schedule(p0);
}

