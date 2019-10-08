#include <arm/mmu.h>

void
vector_table_load(void);

uint32_t
fsr_status(void);

size_t
fault_addr(void);

void
intc_reset(void);

void
set_intr(int code);

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

void *
kernel_map(size_t pa, size_t len, bool cache);

void
kernel_unmap(void *va, size_t len);

void
jump(size_t arg, size_t sp, size_t pc)
	__attribute__((noreturn));

