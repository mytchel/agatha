#include "../../port/head.h"
#include "fns.h"
#include "trap.h"
#include <m.h>

	void
trap(size_t pc, int type)
{
	uint32_t fsr;
	size_t addr;

	debug("trap at 0x%h, type %i\n", pc, type);

	switch(type) {
		case ABORT_INTERRUPT:
			irq_handler();

			return; /* Note the return. */

		case ABORT_INSTRUCTION:
			debug("abort instruction at 0x%h\n", pc);
			break;

		case ABORT_PREFETCH:
			debug("prefetch instruction at 0x%h\n", pc);
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

	if (up == nil) {
		panic("trap with no proc on cpu!!!\n");
	}

	debug("killing proc %i\n", up->pid);

	up->state = PROC_dead;

	schedule(nil);

	/* Never reached */
}


