#define nirq 128

struct intc {
	uint32_t revision;
	uint32_t pad1[3];
	uint32_t sysconfig;
	uint32_t sysstatus;
	uint32_t pad2[10];
	uint32_t sir_irq;
	uint32_t sir_fiq;
	uint32_t control;
	uint32_t protection;
	uint32_t idle;
	uint32_t pad3[3];
	uint32_t irq_priority;
	uint32_t fiq_priority;
	uint32_t threshold;
	uint32_t pad4[5];

	struct {
		uint32_t itr;
		uint32_t mir;
		uint32_t mir_clear;
		uint32_t mir_set;
		uint32_t isr_set;
		uint32_t isr_clear;
		uint32_t pending_irq;
		uint32_t pending_fiq;
	} set[4];

	uint32_t ilr[nirq];
};


