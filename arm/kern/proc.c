#include "../../kern/head.h"
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
  uint8_t m[MESSAGE_LEN];
  uint32_t *r;
  label_t u = {0};

  debug("in proc_start for %i\n", up->pid);

  while (recv(m) != OK) {
    debug("%i proc_start: krecv errored!\n", up->pid);
  }

  r = (uint32_t *) m;
  u.pc = r[0];
  u.sp = r[1];

  debug("drop_to_user %i at 0x%h 0x%h\n", up->pid, u.pc, u.sp);
  drop_to_user(&u, up->kstack, KSTACK_LEN);
}

