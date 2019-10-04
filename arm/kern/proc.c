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
	cap_t *c, *c_main;
	union proc_msg m;
	label_t u = {0};
	int pid;

	debug_info("proc %i starting\n", up->pid);

	while (true) {
		if ((c = recv(nil, &pid, (uint8_t *) &m)) == nil) {
			continue;
		} else if (pid == PID_SIGNAL) {
			continue;
		} else if (m.start.type == PROC_start_req) {
			break;
		}
	}

	debug_info("got start, reply on cid %i\n", c->id);

	c_main = cap_accept();

	m.start.type = PROC_start_rsp;

	reply((obj_endpoint_t *) c->obj, pid, (uint8_t *) &m);

	u.psr = MODE_USR;
	u.pc = m.start.pc;
	u.sp = m.start.sp;

	if (c_main != nil) {
		u.regs[0] = c_main->id;
	} else {
		u.regs[0] = -1;
	}

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

