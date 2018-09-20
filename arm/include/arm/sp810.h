struct sp810_regs {
	uint32_t id;
	uint32_t sw;
	uint32_t led;
	uint32_t osc[5];
	uint32_t lock;
	uint32_t counter_100hz;
	uint32_t cfgdata[2];
	uint32_t flags;
	uint32_t flagsclr;
	uint32_t nvflags;
	uint32_t nvflagsclr;
	uint32_t resetctl;
	uint32_t pcictl;
	uint32_t mci;
	uint32_t flash;
	uint32_t clcd;
	uint32_t clcdser;
	uint32_t bootcs;
	uint32_t counter_24Mhz;
	uint32_t misc;
	uint32_t dmapsr[3];
	uint32_t oscreset[5];
	uint32_t test_osc[5];
};

