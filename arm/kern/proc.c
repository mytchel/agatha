#include "../../sys/head.h"
#include "fns.h"
#include "trap.h"

void
func_label(label_t *l, 
           size_t stack,
           size_t stacklen,
           size_t func)
{
	memset(l, 0, sizeof(struct label));

	l->psr = MODE_SVC;
	l->sp = (uint32_t) stack + stacklen;
	l->pc = (uint32_t) func;
}

void
__attribute__((noreturn))
proc_start(void)
{
	union proc_msg m;
	label_t u = {0};
	endpoint_t *e;
	int mid;

	debug_info("proc %i starting\n", up->pid);

	e = proc_find_endpoint(up, EID_MAIN);
	if (e == nil) {
		panic("proc %i has no main endpoint\n", up->pid);
	}

	while (true) {
		if (recv(e, &mid, (uint8_t *) &m) <= 0) {
			continue;
		} else if (m.start.type == PROC_start_msg) {
			break;
		}
	}

	debug_info("got start, reply on eid %i", e->id);

	reply(e, mid, (uint8_t *) &m);

	u.psr = MODE_USR;
	u.pc = m.start.pc;
	u.sp = m.start.sp;
	u.regs[0] = m.start.arg;

	debug(DEBUG_INFO, "proc %i got start regs at 0x%x 0x%x with arg 0x%x\n", 
			up->pid, u.pc, u.sp, u.regs[0]);

	drop_to_user(&u);
}

	int
mmu_switch(size_t pa)
{
	mmu_invalidate();
	mmu_load_ttb((uint32_t *) pa);

	return OK;
}

