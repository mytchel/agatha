/*
 *
 * Copyright (c) 2017 Mytchel Hammond <mytch@lackname.org>
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "head.h"
#include "fns.h"

/*

Example with device tree blob

pc = 0x82000010
lr = 0x0
r0 = 0x0
r1 = 0xe05
r2 = 0x8fff4000
r3 = 0x8b80
r4 = 0x0
r5 = 0x9ffb32d8
r6 = 0x82000000
r7 = 0x0
r8 = 0x9ff4efe5
r9 = 0x9df2ded8
r10 = 0x82000040
r11 = 0x0
r12 = 0x9df2edaa
r13 = 0x82000010

Example without device tree blob

pc = 0x82000010
lr = 0x0
r0 = 0x0
r1 = 0xe05
r2 = 0x80000100
r3 = 0x9df2dfb0
r4 = 0x0
r5 = 0x9ffb32d8
r6 = 0x82000000
r7 = 0x0
r8 = 0x9ff4efe5
r9 = 0x9df2ded8
r10 = 0x82000040
r11 = 0x0
r12 = 0x9df2edaa
r13 = 0x82000010

	 Register's as they were set by u-boot before loading.
   Set to 1 so it doesn't end up in bss. 
   
   From experimenting it would seem that:
   
   r2 = device tree blob address.
   r3 = device tree blob size. Consistant strange number if no 
   	blob was given.
   r6 = kernel address.
   
   I have no idea what any of the other registers are set
   for. r1, r5, r9, r12 seem to stay consistant regardless of the
   presense of a device tree. r9 changes.
   
   */
   
struct label init_regs = {1};

extern void *_proc0_text_start;
extern void *_proc0_text_end;
extern void *_proc0_data_start;
extern void *_proc0_data_end;

#define PROC0_STACK_TOP  0x8000
#define PROC0_VA_START   0x8000

static uint8_t proc0_stack[PAGE_SIZE * 4]
	__attribute__((__aligned__(PAGE_SIZE))) = { 0 };
	
static uint32_t
proc0_l1[4096]__attribute__((__aligned__(16*1024))) = { 0 };

static uint32_t
proc0_l2[256]__attribute__((__aligned__(PAGE_SIZE))) = { 0 };

static void
proc_nil(void)
{
	debug("in proc_nil\n");
	
	set_intr(INTR_on);
	while (true)
		;
}

static void
proc0_start(void)
{
	label_t u = {0};
	
	debug("starting proc0\n");
	
	u.sp = (reg_t) PROC0_STACK_TOP;
	u.pc = (reg_t) PROC0_VA_START;
	
	drop_to_user(&u, up->kstack, KSTACK_LEN);
}

static proc_t
init_procs(void)
{
	proc_t p;
	size_t s, l;
	kframe_t f;
	
	p = proc_new();
	if (p == nil) {
		panic("Failed to create proc_nil!\n");
	}
	
	func_label(&p->label, p->kstack, KSTACK_LEN, &proc_nil);
	
	p = proc_new();
	if (p == nil) {
		panic("Failed to create proc0!\n");
	}
	
	func_label(&p->label, p->kstack, KSTACK_LEN, &proc0_start);
	
	/* Set up proc0's virtual address space. */

	s = (size_t) proc0_l1;
	l = sizeof(proc0_l1);
	debug("give proc0 0x%h -> 0x%h\n", s, l);
	f = frame_new(p, s, l, F_TYPE_MEM);
	
	debug("map proc0 l1\n"); 
	frame_map(p, f, 0, F_MAP_L1_TABLE);
	
	s = (size_t) proc0_l2;
	l = sizeof(proc0_l2);
	debug("give proc0 0x%h -> 0x%h\n", s, l);
	f = frame_new(p, s, l, F_TYPE_MEM);
	
	debug("map proc0 l2\n"); 
	frame_map(p, f, 0, F_MAP_L2_TABLE);
	          
	s = (size_t) &_proc0_text_start;
	l = (size_t) &_proc0_data_end - s;
	debug("give proc0 0x%h -> 0x%h\n", s, l);
	f = frame_new(p, s, l, F_TYPE_MEM);
	
	debug("map proc0 text/data to 0x%h\n", PROC0_VA_START); 
	frame_map(p, f, PROC0_VA_START, F_MAP_READ|F_MAP_WRITE);
	
	s = (size_t) proc0_stack;
	l = sizeof(proc0_stack);
	debug("give proc0 0x%h -> 0x%h\n", s, l);
	f = frame_new(p, s, l, F_TYPE_MEM);
	
	debug("map proc0 stack to 0x%h\n", PROC0_STACK_TOP - sizeof(proc0_stack)); 
	frame_map(p, f, PROC0_STACK_TOP - sizeof(proc0_stack),
	          F_MAP_READ|F_MAP_WRITE);
		
	/* Give proc0 all remaining memory. */
	
	s = (size_t) &_ram_start;
	l = SECTION_ALIGN_DN((size_t) &_kernel_start) - s;
	debug("give proc0 0x%h -> 0x%h\n", s, l);
	frame_new(p, s, l, F_TYPE_MEM);
	
	s = SECTION_ALIGN((size_t) &_kernel_end);
	l = (size_t) &_ram_end - s;
	debug("give proc0 0x%h -> 0x%h\n", s, l);
	frame_new(p, s, l, F_TYPE_MEM);
	
	/* TODO: add io register maps. */
	
	return p;
}

int
kmain(void)
{
	proc_t p;
		
	/* TODO: These should be small pages not sections. */
	
	imap(0x80000000, 0x20000000, AP_RW_NO, true); 
	
  /* INTCPS */
	imap(0x48200000, 0x1000, AP_RW_NO, false); 
	init_intc((void *) 0x48200000);
	
	/* Watchdog */
  imap(0x44E35000, 0x1000, AP_RW_NO, false);
  init_watchdog((void *) 0x44E35000);
	
  /* DMTIMER2 for systick. */
  imap(0x48040000, 0x1000, AP_RW_NO, false); 
  imap(0x44E00500, 0x100, AP_RW_NO, false); 
  init_timers((void *) 0x48040000, 68, (void *) 0x44E00500);
	
	/* UART0 */
  imap(0x44E09000, 0x1000, AP_RW_NO, false);
  init_uart((void *) 0x44E09000);

	init_mmu();
	
	p = init_procs();
			
	schedule(p);
  
  /* Never reached */
  return 0;
}

