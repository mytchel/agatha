struct cortex_a9_gic_dst_regs {
	uint32_t dcr;
	uint32_t ictr;
	uint32_t diir;
	uint32_t pad0[29];
	uint32_t isr[32];
	uint32_t ise[32];
	uint32_t ice[32];
	uint32_t isp[32];
	uint32_t icp[32];
	uint32_t asr[32];
	uint32_t pad1[32];
	uint32_t ipr[256];
	uint32_t spi[256];
	uint32_t icr[64];
	uint32_t pad2[128];
	uint32_t icdsgir;
};

struct cortex_a9_gic_cpu_regs {
	volatile uint32_t control;
	volatile uint32_t priority;
	volatile uint32_t bin_pt;
	volatile uint32_t ack;
	volatile uint32_t eoi;
	volatile uint32_t run_priority;
	volatile uint32_t hi_pending;
	volatile uint32_t alias_bin_pt_ns;
	volatile uint32_t pad0[55];
	volatile uint32_t implementation;
};

