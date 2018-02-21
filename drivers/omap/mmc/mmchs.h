typedef enum { MMC_EMMC, MMC_SD } mmc_t;

extern volatile struct mmchs_regs *regs;
extern char *name;
extern int intr;
extern uint32_t csd[4];
extern uint32_t rca;
extern uint32_t nblk;

bool
mmchssendcmd(uint32_t cmd, uint32_t arg);

bool
mmchssendappcmd(uint32_t cmd, uint32_t arg);

bool
mmchssoftreset(void);

void
mmcreset(uint32_t line);

bool
cardgotoidle(void);

bool
cardidentification(void);

bool
cardqueryvolttype(void);

bool
cardidentify(void);

bool
cardselect(void);

bool
cardcsd(void);

bool
mmchsreaddata(uint32_t *buf, size_t l);

bool
mmchswritedata(uint32_t *buf, size_t l);

bool
mmcinit(void);

bool
sdinit(void);

bool
readblock(uint32_t blk, uint8_t *buf);

bool
writeblock(uint32_t blk, uint8_t *buf);

int
__bitfield(uint32_t *src, int start, int len);

void printf(const char *, ...);

uint32_t
intcopylittle32(uint8_t *src);

