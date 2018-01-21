struct kernel_devices {
	void (*init_intc)(void);

	void (*trap)(size_t pc, int type);
	int (*add_kernel_irq)(size_t irqn, void (*func)(size_t));
	int (*add_user_irq)(size_t irqn, proc_t p);

	void (*init_timer)(void);
	void (*timer)(size_t ms);

	void (*init_debug)(void);
	void (*debug)(const char *str);
};

uint32_t
fsr_status(void);

size_t
fault_addr(void);

void
intc_add_handler(uint32_t irq,
                 void (*func)(uint32_t));

void
intc_reset(void);

void
mmu_invalidate(void);

void
mmu_enable(void);

void
mmu_disable(void);

void
mmu_load_ttb(uint32_t *);

int
map_l2(uint32_t *l1, size_t pa, size_t va);

#define AP_NO_NO	0
#define AP_RW_NO	1
#define AP_RW_RO	2
#define AP_RW_RW	3

int
map_pages(uint32_t *l2, size_t pa, size_t va, 
          size_t len, int ap, bool cache);

int
unmap_pages(uint32_t *l2, size_t va, size_t len);

int
map_sections(uint32_t *l1, size_t pa, size_t va, 
             size_t len, int ap, bool cache);

int
unmap_sections(uint32_t *l1, size_t va, size_t len);

void
serial_puts(const char *c);

void
map_devs(void *dtb);

void
init_devs(void);

extern uint32_t kernel_ttb[], kernel_l2[];
extern size_t kernel_va_slot;

extern struct kernel_devices kernel_devices;

