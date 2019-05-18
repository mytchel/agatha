struct gic_dst_regs {
	uint32_t control;
	uint32_t type;
	uint32_t iid;
	uint32_t pad0[29];
	uint32_t igroup[32];
	uint32_t isenable[32];
	uint32_t icenable[32];
	uint32_t ispend[32];
	uint32_t icpend[32];
	uint32_t isactive[32];
	uint32_t icactive[32];
	uint32_t ipriority[64];
	uint32_t itargets[64]; /* First 8 are special and ro */
	uint32_t pad1[192];
	uint32_t icfg[16];
	uint32_t pad2[48];
	uint32_t ppis;
	uint32_t spis[7];
	uint32_t pad3[120];
	uint32_t sgi;
	uint32_t pad4[3];
	uint32_t cpendsgi[4];
	uint32_t spendsgi[4];
	uint32_t ident[12];
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
	uint32_t alias_ack;
	uint32_t alias_eoi;
	uint32_t alias_hi_pending;
	uint32_t pad0[41];
	uint32_t apr;
	uint32_t pad1[3];
	uint32_t nsapr0;
	uint32_t pad2[6];
	uint32_t idr;
	uint32_t pad3[960];
	uint32_t dir;
};

