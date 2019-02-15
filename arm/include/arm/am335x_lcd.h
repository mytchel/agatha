struct am335x_lcd_regs {
  uint32_t pid;
  uint32_t ctrl;
  uint32_t pad0[1];
  uint32_t lidd_ctrl;
	struct {
		uint32_t conf;
		uint32_t addr;
		uint32_t data;
	} lidd_cs[2];
  uint32_t raster_ctrl;
  uint32_t raster_timing[3];
  uint32_t raster_subpanel[2];
  uint32_t lcddma_ctrl;
	struct {
		uint32_t base;
		uint32_t ceiling;
	} lcddma_fb[2];
  uint32_t sysconfig;
  uint32_t irqstatus_raw;
  uint32_t irqstatus;
  uint32_t irqenable_set;
  uint32_t irqenable_clr;
  uint32_t clkc_enable;
  uint32_t clkc_reset;
};

