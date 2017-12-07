
struct gpio_regs {
  uint32_t rev;
  uint32_t pad1[3];
  uint32_t sysconfig;
  uint32_t pad2[3];
  uint32_t eoi;
  uint32_t irqstatus_raw_0;
  uint32_t irqstatus_raw_1;
  uint32_t irqstatus_0;
  uint32_t irqstatus_1;
  uint32_t irqstatus_set_0;
  uint32_t irqstatus_set_1;
  uint32_t irqstatus_clr_0;
  uint32_t irqstatus_clr_1;
  uint32_t irqwaken_0;
  uint32_t irqwaken_1;
  uint32_t pad3[50];
  uint32_t sysstatus;
  uint32_t pad4[6];
  uint32_t ctrl;
  uint32_t oe;
  uint32_t datain;
  uint32_t dataout;
  uint32_t leveldetect0;
  uint32_t leveldetect1;
  uint32_t risingdetect;
  uint32_t fallingdetect;
  uint32_t debounceenable;
  uint32_t debouncingtime;
  uint32_t pad5[14];
  uint32_t cleardataout;
  uint32_t setdataout;
};

