struct pl18x_regs {
	uint32_t power;
	uint32_t clock;
	uint32_t argument;
	uint32_t command;
	uint32_t respcommand;
	uint32_t response[4];
	uint32_t datatimer;
	uint32_t datalength;
	uint32_t datactrl;
	uint32_t status;
	uint32_t status_clear;
	uint32_t mask[2];
	uint32_t card_select;
	uint32_t fifo_count;
	uint32_t pad0[(0x80 - 0x4c)>>2];
	uint32_t fifo[16];
	uint32_t pad1[(0xfe0 - 0x84)>>2];
	uint32_t periph_id[4];
	uint32_t pcell_id[4];
};


