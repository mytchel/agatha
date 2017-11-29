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

static uint8_t proc0_stack[PAGE_SIZE]__attribute__((__aligned__(PAGE_SIZE)));

static void
proc0_start(void)
{
	label_t u = {0};
	
	debug("starting proc0\n");
	
	u.sp = (reg_t) proc0_stack + sizeof(proc0_stack);
	u.pc = (reg_t) &_proc0_text_start;
	
	drop_to_user(&u, up->kstack, KSTACK_LEN);
}

static proc_t
init_proc0(void)
{
	proc_t p;
	
	p = proc_new();
	if (p == nil) {
		panic("Failed to create proc0!\n");
	}
	
	func_label(&p->label, p->kstack, KSTACK_LEN, &proc0_start);
	
	return p;
}

int
kmain(void)
{
	proc_t p0;
	
  debug("OMB Booting...\n");

	init_intc();
	init_watchdog();
	init_timers();
	init_memory();
	
	p0 = init_proc0();
		
	schedule(p0);
  
  /* Never reached */
  return 0;
}

