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
  uint32_t m[MESSAGE_LEN/sizeof(uint32_t)];
  label_t u = {0};
  int p;

  while ((p = recv((uint8_t *) m)) < 0) 
		;

  u.psr = MODE_USR;
  u.pc = m[0];
  u.sp = m[1];

  drop_to_user(&u, up->kstack, KSTACK_LEN);
}

