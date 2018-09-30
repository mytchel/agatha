struct gic_dst_regs {
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

struct gic_cpu_regs {
	uint32_t control;
	uint32_t priority;
	uint32_t bin_pt;
	uint32_t ack;
	uint32_t eoi;
	uint32_t run_priority;
	uint32_t hi_pending;
	uint32_t alias_bin_pt_ns;
	uint32_t pad0[55];
	uint32_t implementation;
};

