void *
map_frame(int f_id, int flags);

int
get_mem_frame(size_t len, size_t align);

size_t
va_to_pa(size_t va);

bool
find_tables(void);

bool
setup_mem(void);

bool
init_procs(void);

extern struct kernel_info *k_info;
extern void *dtb;

extern struct frame l1, l2;

