struct cortex_a8_intr_regs {
	uint32_t revision;
	uint8_t  pad0[12];
	uint32_t sysconfig;
	uint32_t sysstatus;
	uint8_t  pad1[40];
	uint32_t sir_irq;
	uint32_t sir_fiq;
	uint32_t control;
	uint32_t protection;
	uint32_t idle;
	uint8_t  pad2[12];
	uint32_t irq_priority;
	uint32_t fiq_priority;
	uint32_t threshold;
	uint8_t  pad3[20];
	struct {
		uint32_t itr;
		uint32_t mir;
		uint32_t mir_clear;
		uint32_t mir_set;
		uint32_t isr_set;
		uint32_t isr_set_clear;
		uint32_t pending_irq;
		uint32_t pending_fiq;
	} bank[4];
	uint32_t ilr[128];
};

