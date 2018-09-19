struct cortex_a9_pt_wd_regs {
	uint32_t t_load;
	uint32_t t_count;
	uint32_t t_control;
	uint32_t t_intr;
	uint32_t pad0[4];
	uint32_t wd_load;
	uint32_t wd_counter;
	uint32_t wd_control;
	uint32_t wd_intr;
	uint32_t wd_reset;
	uint32_t wd_disable;
};

