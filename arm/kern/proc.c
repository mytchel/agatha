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
	int pid, c_main;
	cap_t *c;

	debug_info("proc %i starting\n", up->pid);

	while (true) {
		if ((c = recv(nil, &pid, (uint8_t *) &m, &c_main)) == nil) {
			continue;
		} else if (pid == PID_SIGNAL) {
			continue;
		} else if (m.start.type == PROC_start_req) {
			break;
		}
	}

	debug_info("got start, reply on cid %i\n", c->id);

	m.start.type = PROC_start_rsp;

	reply((obj_endpoint_t *) c->obj, pid, (uint8_t *) &m, 0);

	u.psr = MODE_USR;
	u.pc = m.start.pc;
	u.sp = m.start.sp;

	u.regs[0] = c_main;

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

