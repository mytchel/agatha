#define I2C_CON_EN    (1<<15)
#define I2C_CON_MST   (1<<10)
#define I2C_CON_TRX   (1<<9)
#define I2C_CON_STP   (1<<1)
#define I2C_CON_STT   (1<<0)

#define I2C_IRQ_BB    (1<<12)
#define I2C_IRQ_BF    (1<<8)
#define I2C_IRQ_XRDY  (1<<4)
#define I2C_IRQ_RRDY  (1<<3)
#define I2C_IRQ_ARDY  (1<<2)
#define I2C_IRQ_NACK  (1<<1)

struct am335x_i2c_regs {
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
	uint32_t buf;
	uint32_t cnt;
	uint32_t data;
	uint32_t pad3[1];
	uint32_t con;
	uint32_t oa;
	uint32_t sa;
	uint32_t psc;
	uint32_t scll;
	uint32_t sclh;
	uint32_t systest;
	uint32_t bufstat;
	uint32_t qa1;
	uint32_t qa2;
	uint32_t qa3;
	uint32_t actoa;
	uint32_t sblock;
};

