
struct i2c_regs {
	uint32_t revnb_lo;
	uint32_t revnb_hi;
	uint32_t pad0[2];
	uint32_t sysc;
	uint32_t pad1[4];
	uint32_t irqstatus_raw;
	uint32_t irqstatus;
	uint32_t irqenable_set;
	uint32_t irqenable_clr;
	uint32_t we;
	uint32_t dmarxenable_set;
	uint32_t dmatxenable_set;
	uint32_t dmarxenable_clr;
	uint32_t dmatxenable_clr;
	uint32_t dmarxwake_en;
	uint32_t dmatxwake_en;
	uint32_t pad2[16];
	uint32_t syss;
	uint32_t cnt;
	uint32_t data;
	uint32_t pad3[1];
	uint32_t con;
	uint32_t oa;
	uint32_t sa;
	uint32_t psc;
	uint32_t scll;
	uint32_t systest;
	uint32_t bufstat;
	uint32_t oa1;
	uint32_t oa2;
	uint32_t oa3;
	uint32_t actoa;
	uint32_t sblock;
};

