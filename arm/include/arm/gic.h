struct gic_dst_regs {
	uint32_t control;
	uint32_t type;
	uint32_t iid;
	uint32_t pad0[29];
	uint32_t group[32];
	uint32_t isenable[32];
	uint32_t icenable[32];
	uint32_t ispend[32];
	uint32_t icpend[32];
	uint32_t isactive[32];
	uint32_t icactive[32];
	uint32_t ipriority[256];
	uint32_t itargets[256];
	uint32_t icfg[64];
	uint32_t pad1[64];
	uint32_t nsac[64];
	/* I don't know how much padding should be here */
	uint32_t sgi;
	uint32_t cpendsgi[4];
	uint32_t spendsgi[4];
	/* some number? */
	uint32_t ident[];
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

