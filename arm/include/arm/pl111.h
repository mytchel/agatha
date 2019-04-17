struct pl111_regs {
	uint32_t timing[4];
	uint32_t upbase;
	uint32_t lpbase;
	uint32_t control;
	uint32_t imsc;
	uint32_t ris;
	uint32_t mis;
	uint32_t icr;
	uint32_t upcurr;
	uint32_t lpcurr;
	uint32_t pad0[115];
	uint32_t palette[128];
	uint32_t pad1[256];
	uint32_t cursor_image[256];
	uint32_t crsr_ctrl;
	uint32_t crsr_config;
	uint32_t crcr_palette0;
	uint32_t crcr_palette1;
	uint32_t crsr_xy;
	uint32_t crsr_clip;
	uint32_t pad2[2];
	uint32_t crsr_imsc;
	uint32_t crsr_icr;
	uint32_t crsr_ris;
	uint32_t crsr_mis;
	uint32_t pad3[236];
	uint32_t periph_id[4];
	uint32_t pcell_id[4];
};

#define CLCD_INT_ERROR  (1<<4)
#define CLCD_INT_VCOMP  (1<<3)
#define CLCD_INT_BASE   (1<<2)
#define CLCD_INT_FIFO   (1<<1)

