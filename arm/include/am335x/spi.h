
struct spi_regs {
  uint32_t revision;
  uint8_t pad0[0x10c];
  uint32_t sysconfig;
  uint32_t sysstatus;
  uint32_t irqstatus;
  uint32_t irqenable;
  uint8_t pad1[0x8];
  uint32_t syst;
  uint32_t modulctrl;

  struct {
    uint32_t conf;
    uint32_t stat;
    uint32_t ctrl;
    uint32_t tx;
    uint32_t rx;
  } ch[4];

  uint32_t xferlevel;
  uint32_t daftx;
  uint32_t dafrx;
};

