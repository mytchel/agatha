struct cortex_a9_pt_wd_regs {
	volatile uint32_t t_load;
	volatile uint32_t t_count;
	volatile uint32_t t_control;
	volatile uint32_t t_intr;
	volatile uint32_t pad0[4];
	volatile uint32_t wd_load;
	volatile uint32_t wd_counter;
	volatile uint32_t wd_control;
	volatile uint32_t wd_intr;
	volatile uint32_t wd_reset;
	volatile uint32_t wd_disable;
};

