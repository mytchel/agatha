struct am335x_dmtimer_regs {
  uint32_t tidr;
  uint8_t pad0[12];
  uint32_t tiocp_cfg;
  uint8_t pad1[12];
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

