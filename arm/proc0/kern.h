/* proc0 only syscalls */

int
proc_new(size_t l1_pa);

int
intr_register(struct intr_mapping *map);

int
kern_debug(char *s);

