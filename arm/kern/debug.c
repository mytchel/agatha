#include "../../sys/head.h"

void
debug_dump_label(label_t *l)
{
	int i;

	debug_info("psr = 0x%x\n", l->psr);	
	debug_info("sp = 0x%x\n", l->sp);	
	debug_info("lr = 0x%x\n", l->lr);	

	for (i = 0; i < 13; i++)
	 debug_info("r%i = 0x%x\n", i, l->regs[i]);	

	debug_info("pc = 0x%x\n", l->pc);	
}

