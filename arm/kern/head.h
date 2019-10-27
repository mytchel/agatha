#include "../../sys/head.h"

bool
intr_cap_claim(size_t irqn);

void
intr_cap_connect(size_t irqn, obj_endpoint_t *e, uint32_t s);

int
irq_add_kernel(size_t irqn, void (*func)(size_t));

void
irq_enable(size_t irqn);

void
irq_ack(size_t irqn);

extern uint32_t *kernel_l1;

