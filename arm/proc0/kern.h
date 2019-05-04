/* proc0 only syscalls */

int
proc_new(void);

int
va_table(int pid, size_t pa);

int
intr_register(struct intr_mapping *map);

int
kern_debug(char *s);

