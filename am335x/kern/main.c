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

extern void *_proc0_text_start;
extern void *_proc0_text_end;
extern void *_proc0_data_start;
extern void *_proc0_data_end;
/*
static uint8_t proc0_stack[PAGE_SIZE]__attribute__((__aligned__(PAGE_SIZE)));
static uint8_t proc1_stack[PAGE_SIZE]__attribute__((__aligned__(PAGE_SIZE)));
*/
static void
proc0_start(void)
{
/*
	label_t u = {0};
	
	debug("starting proc0\n");
	
	u.sp = (reg_t) proc0_stack + sizeof(proc0_stack);
	u.pc = (reg_t) &_proc0_text_start;
	
	drop_to_user(&u, up->kstack, KSTACK_LEN);
	*/
	
	uint8_t m[MESSAGE_LEN];
	
	*m = 0;
	
	while (true) {
		debug("%i send/recv %i\n", up->pid, *m);
		send(find_proc(1), m);
		recv(m);		
	}
}

static void
proc1_start(void)
{
/*
	label_t u = {0};
	
	debug("starting proc1\n");
	
	u.sp = (reg_t) proc1_stack + sizeof(proc1_stack);
	u.pc = (reg_t) &_proc0_text_start;
	
	drop_to_user(&u, up->kstack, KSTACK_LEN);
	*/
	
	
	uint8_t m[MESSAGE_LEN];
	int p;
	
	while (true) {
		debug("%i send/recv %i\n", up->pid, *m);
		p = recv(m);
		(*m)++;
		send(find_proc(p), m);	
	}
}

static void
init_procs(void)
{
	proc_t p;
	
	p = proc_new();
	if (p == nil) {
		panic("Failed to create proc0!\n");
	}
	
	func_label(&p->label, p->kstack, KSTACK_LEN, &proc0_start);
	
	p = proc_new();
	if (p == nil) {
		panic("Failed to create proc1!\n");
	}
	
	func_label(&p->label, p->kstack, KSTACK_LEN, &proc1_start);
}

int
kmain(void)
{
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
		
	init_procs();
			
	schedule(nil);
  
  /* Never reached */
  return 0;
}

