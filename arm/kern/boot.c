#include <types.h>
#include <mach.h>
#include "fns.h"
#include "trap.h"
#include "c.h"

uint32_t
kernel_info[1024] __attribute__((__aligned__(0x1000))) = { 0 };

uint32_t
kernel_l1[4096]__attribute__((__aligned__(0x4000))) = { 0 };

uint32_t
kernel_l2[1024]__attribute__((__aligned__(0x1000))) = { 0 };

uint32_t
proc0_stack[1024]__attribute__((__aligned__(0x1000))) = { 0 };

uint32_t
proc0_l1[4096]__attribute__((__aligned__(0x4000))) = { 0 };

uint32_t
proc0_l2[1024]__attribute__((__aligned__(0x1000))) = { 0 };

extern uint32_t *_boot_start;
extern uint32_t *_boot_end;
extern uint32_t *_image_start;
extern uint32_t *_image_end;
extern uint32_t *_binary_kernel_bin_start;
extern uint32_t *_binary_kernel_bin_end;
extern uint32_t *_binary_proc0_bin_start;
extern uint32_t *_binary_proc0_bin_end;
extern uint32_t *_binary_bundle_bin_start;
extern uint32_t *_binary_bundle_bin_end;

	void
main(uint32_t j)
{
	static struct kernel_info *info = (struct kernel_info *) kernel_info;

	memset(info, 0, sizeof(struct kernel_info));

	info->kernel_pa    = (size_t) &_binary_kernel_bin_start;
	info->kernel_len   = 
		PAGE_ALIGN(&_binary_kernel_bin_end) - 
		info->kernel_pa;

	info->info_pa = (size_t) kernel_info;
	info->info_len = sizeof(kernel_info);

	info->bundle_pa = (size_t) &_binary_bundle_bin_start;
	info->bundle_len   = 
		PAGE_ALIGN((size_t) &_binary_bundle_bin_end) - 
		info->bundle_pa;

	info->proc0.prog_pa = (size_t) &_binary_proc0_bin_start;
	info->proc0.prog_len = 
		PAGE_ALIGN((size_t) &_binary_proc0_bin_end) -
		info->proc0.prog_pa;

	info->proc0.stack_pa = (size_t) proc0_stack;
	info->proc0.stack_len = sizeof(proc0_stack);

	info->proc0.l1_pa = (size_t) proc0_l1;
	info->proc0.l1_len = sizeof(proc0_l1);

	info->proc0.l2_pa = (size_t) proc0_l2;
	info->proc0.l2_len = sizeof(proc0_l2);

	info->kernel.l1_pa = (size_t) kernel_l1;
	info->kernel.l1_len = sizeof(kernel_l1);

	info->kernel.l2_pa = (size_t) kernel_l2;
	info->kernel.l2_len = sizeof(kernel_l2);

	info->kernel_va = 0xff000000;

	info->kernel.info_va = 
		info->kernel_va + info->kernel_len;

	info->kernel.l1_va = (uint32_t *) 
		(info->kernel.info_va + info->info_len);

	info->kernel.l2_va = (uint32_t *) 
		(((size_t) info->kernel.l1_va) + info->kernel.l1_len);

	info->boot_pa = (size_t) &_boot_start;
	info->boot_len = (size_t) &_boot_end -
		info->boot_pa;

	/* Set up temporary mapping to this code
		 that will be unmapped once we jump into
		 the kernel */
	uint32_t *tmp_table = (uint32_t *) PAGE_ALIGN(&_image_end);

	map_l2(kernel_l1, (size_t) tmp_table, 
			TABLE_ALIGN(info->boot_pa), 0x1000);
	
	map_pages(tmp_table, 
			info->boot_pa, 
			info->boot_pa, 
			info->boot_len, 
			AP_RW_RW, false);

	map_l2(kernel_l1, 
			(size_t) kernel_l2, 
			info->kernel_va,
		 	0x1000);

	map_pages(kernel_l2, 
			info->kernel_pa, 
			info->kernel_va, 
			info->kernel_len, 
			AP_RW_NO, true);

	map_pages(kernel_l2, 
			info->info_pa, 
			info->kernel.info_va, 
			info->info_len, 
			AP_RW_NO, true);

	map_pages(kernel_l2, 
			info->kernel.l1_pa, 
			(size_t) info->kernel.l1_va, 
			info->kernel.l1_len, 
			AP_RW_NO, true);

	map_pages(kernel_l2, 
			info->kernel.l2_pa,
			(size_t) info->kernel.l2_va, 
			info->kernel.l2_len, 
			AP_RW_NO, true);

	mmu_load_ttb((uint32_t *) info->kernel.l1_pa);
	mmu_invalidate();
	mmu_enable();

	jump(info->kernel.info_va, 
			0,
			info->kernel_va);
}

