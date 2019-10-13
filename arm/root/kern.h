/* proc0 only syscalls */

int
kern_debug(char *s);

int
intr_create(size_t irqn);

int
obj_create(size_t pa, size_t len);

