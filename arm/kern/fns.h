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

/* Initialisation functions */

void
map_ti_am33xx_intc(void *dtb);

void
init_ti_am33xx_intc(void);

void
map_ti_am335x_uart(void *dtb);

void
init_ti_am335x_uart(void);

void
map_devs(void *dtb);

void
init_devs(void);

extern uint32_t kernel_ttb[], kernel_l2[];
extern size_t kernel_va_slot;

