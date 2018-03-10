#include "mem.h"

uint32_t
fsr_status(void);

size_t
fault_addr(void);

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

void
map_devs(void);

void
init_devs(void);

extern uint32_t kernel_ttb[], kernel_l2[];
extern size_t kernel_va_slot;

extern struct kernel_devices kernel_devices;

