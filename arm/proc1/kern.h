/* proc0 only syscalls */

int
proc_new(int supervisor_pid,
	int supervisor_eid, 
	int priority,
	size_t l1_pa);

int
proc_setup(int pid, procstate_t state);

int
endpoint_create(int pid);

int
endpoint_connect(int from, int to, int to_eid);

int
intr_register(struct intr_mapping *map);

int
kern_debug(char *s);

