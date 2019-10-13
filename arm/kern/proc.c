#include "head.h"
#include "fns.h"
#include "trap.h"

void
func_label_h(void);

void
func_label(label_t *l, 
           size_t stack,
           size_t stacklen,
           size_t func,
           size_t arg0,
           size_t arg1)
{
	memset(l, 0, sizeof(struct label));

	uint32_t *s = (uint32_t *) (stack + stacklen);
	*(--s) = func;
	*(--s) = arg1;
	*(--s) = arg0;

	l->psr = MODE_SVC;
	l->sp = (uint32_t) s;
	l->pc = (uint32_t) &func_label_h;
}

void
__attribute__((noreturn))
proc_start(size_t pc, size_t sp)
{
	label_t u;

	u.psr = MODE_USR;
	u.pc = pc;
	u.sp = sp;

	debug(DEBUG_INFO, "proc %i start with pc=0x%x sp=0x%x\n", 
			up->pid, u.pc, u.sp);

	drop_to_user(&u);
}

	int
mmu_switch(size_t pa)
{
	mmu_invalidate();
	mmu_load_ttb((uint32_t *) pa);

	return OK;
}

