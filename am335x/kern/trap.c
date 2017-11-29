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

#include <head.h>
#include "fns.h"
#include "trap.h"

#define nirq 128
#define INTC			0x48200000

struct intc {
	uint32_t revision;
	uint32_t pad1[3];
	uint32_t sysconfig;
	uint32_t sysstatus;
	uint32_t pad2[10];
	uint32_t sir_irq;
	uint32_t sir_fiq;
	uint32_t control;
	uint32_t protection;
	uint32_t idle;
	uint32_t pad3[3];
	uint32_t irq_priority;
	uint32_t fiq_priority;
	uint32_t threshold;
	uint32_t pad4[5];
	
	struct {
		uint32_t itr;
		uint32_t mir;
		uint32_t mir_clear;
		uint32_t mir_set;
		uint32_t isr_set;
		uint32_t isr_clear;
		uint32_t pending_irq;
		uint32_t pending_fiq;
	} set[4];
	
	uint32_t ilr[nirq];
};

struct intc *intc = (struct intc *) INTC;

static void (*handlers[nirq])(uint32_t) = {nil};

void
init_intc(void)
{
  int i;
	
  /* enable interface auto idle */
  intc->sysconfig = 1;

  /* mask all interrupts. */
  for (i = 0; i < 4; i++) {
  	intc->set[i].mir = 0xffffffff;
  }
	
  /* Set all interrupts to lowest priority. */
  for (i = 0; i < nirq; i++) {
  	intc->ilr[i] = 63 << 2;
  }
	
	intc->control = 1;
}

void
mask_intr(uint32_t irqn)
{
  uint32_t mask, mfield;

  mfield = irqn / 32;
  mask = 1 << (irqn % 32);

	intc->set[mfield].mir_set = mask;
}

void
unmask_intr(uint32_t irqn)
{
  uint32_t mask, mfield;

  mfield = irqn / 32;
  mask = 1 << (irqn % 32);

	intc->set[mfield].mir_clear = mask;
}

void
intc_add_handler(uint32_t irqn, void (*func)(uint32_t))
{
  handlers[irqn] = func;
  unmask_intr(irqn);
}

void
intc_reset(void)
{
	intc->control = 1;
}

static void
irq_handler(void)
{
  uint32_t irq;
	
  irq = intc->sir_irq;
  
  if (handlers[irq]) {
    handlers[irq](irq);
  } else {
    debug("no handler\n");
  }
}

void
trap(reg_t pc, int type)
{
  uint32_t fsr;
  reg_t addr;

  switch(type) {
  case ABORT_INTERRUPT:
    irq_handler();

    return; /* Note the return. */

  case ABORT_INSTRUCTION:
    debug("bad instruction at 0x%h\n", pc);
    break;

  case ABORT_PREFETCH:
    debug("prefetch abort at 0x%h\n", pc);
    break;

  case ABORT_DATA:
    addr = fault_addr();
    fsr = fsr_status() & 0xf;

    debug("data abort at 0x%h for 0x%h type 0x%h\n", pc, addr, fsr);
    
    switch (fsr) {
    case 0x5: /* section translation */
    case 0x7: /* page translation */

    case 0x0: /* vector */
    case 0x1: /* alignment */
    case 0x3: /* also alignment */
    case 0x2: /* terminal */
    case 0x4: /* external linefetch section */
    case 0x6: /* external linefetch page */
    case 0x8: /* external non linefetch section */
    case 0xa: /* external non linefetch page */
    case 0x9: /* domain section */
    case 0xb: /* domain page */
    case 0xc: /* external translation l1 */
    case 0xe: /* external translation l2 */
    case 0xd: /* section permission */
    case 0xf: /* page permission */
    default:
      break;
    }

    break;
  }

	debug("trap doesn't know what to do");
	if (up != nil)
		debug(" with proc %i!\n", up->pid);
	else
		debug("!\n");
		
	while (true)
		;
}

