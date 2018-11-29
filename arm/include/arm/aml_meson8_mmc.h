struct aml_meson8_mmc_regs {
	uint32_t argu;
	uint32_t send;
	uint32_t cntl;
	uint32_t stat;
	uint32_t clkc;
	uint32_t addr;
	uint32_t pdma;
	uint32_t misc;
	uint32_t data;
	uint32_t ictl;
	uint32_t ista;
	uint32_t srst;
};

#define ISTA_ICR_MASK    0x3fff
#define ISTA_COK         0x00000001
#define ISTA_CTIMEOUT    0x00000002
#define ISTA_CCRCFAIL    0x00000004
#define ISTA_DATAC       0x00000008
#define ISTA_DOK         0x00000010
#define ISTA_DTIMEOUT    0x00000020
#define ISTA_DCRCFAIL    0x00000040
#define ISTA_DTOK        0x00000080
#define ISTA_RXACT       0x00000100
#define ISTA_TXACT       0x00000200
#define ISTA_DAT1_INT    0x00000400
#define ISTA_DMAOK       0x00000800
#define ISTA_RXFULL      0x00001000
#define ISTA_TXEMPTY     0x00002000
#define ISTA_DAT1_INT_A  0x00004000

#define CMD_CMDINDEX_MASK  0x3f
#define CMD_HAS_RESP       0x40
#define CMD_HAS_DATA       0x80
#define CMD_LONGRESP      0x100
#define CMD_NO_CRC        0x200
#define CMD_DREAD         0x000
#define CMD_DWRITE        0x400
#define CMD_DSTOP         0x800

#define COMMAND_REG_DELAY 300

