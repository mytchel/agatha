struct sp804_timer_regs {
	struct {
		uint32_t load;
		uint32_t cur;
#define SP804_CTRL_ENABLE     (1<<7)
#define SP804_CTRL_MODE       (1<<6)
#define SP804_CTRL_INT_ENABLE (1<<5)
#define SP804_CTRL_PRE        (1<<2)
#define SP804_CTRL_SIZE       (1<<1)
#define SP804_CTRL_ONE_SHOT   (1<<0)
		uint32_t ctrl;
		uint32_t clr;
		uint32_t status_raw;
		uint32_t status;
		uint32_t background_load;
	} t[2];
};

