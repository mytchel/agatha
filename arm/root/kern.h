/* proc0 only syscalls */

int
proc_new(int priority, size_t l1_pa,
	int *p_id, int *e_id);

int
proc_setup(int pid, procstate_t state);

int
kern_debug(char *s);

int
intr_create(size_t irqn);

