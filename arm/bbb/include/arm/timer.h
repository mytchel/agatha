struct timer {
	uint32_t tidr;
	uint32_t pad1[3];
	uint32_t tiocp_cfg;
	uint32_t pad2[3];
	uint32_t irq_eoi;
	uint32_t irqstatus_raw;
	uint32_t irqstatus;
	uint32_t irqenable_set;
	uint32_t irqenable_clr;
	uint32_t irqwakeen;
	uint32_t tclr;
	uint32_t tcrr;
	uint32_t tldr;
	uint32_t ttgr;
	uint32_t twps;
	uint32_t tmar;
	uint32_t tcar1;
	uint32_t tsicr;
	uint32_t tcar2;
};

struct cm_dpll {
	uint32_t pad1[1];
	uint32_t timer7_clk;
	uint32_t timer2_clk;
	uint32_t timer3_clk;
	uint32_t timer4_clk;
	uint32_t mac_clksel;
	uint32_t timer5_clk;
	uint32_t timer6_clk;
	uint32_t cpts_rtf_clksel;
	uint32_t timer1ms_clk;
	uint32_t gfx_fclk;
	uint32_t pru_icss_ocp_clk;
	uint32_t lcdc_pixel_clk;
	uint32_t wdt1_clk;
	uint32_t gpio0_bdclk;
};

