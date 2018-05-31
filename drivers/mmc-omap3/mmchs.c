#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <proc0.h>
#include <stdarg.h>
#include <string.h>
#include <am335x/mmchs.h>

#include "sdmmcreg.h"
#include "sdhcreg.h"
#include "mmchs.h"

volatile struct mmchs_regs *regs = nil;
char *name = nil;
int intr = 0;
uint32_t csd[4] = {0};
uint32_t rca = 0;
uint32_t nblk = 0;
mmc_t type;

void
printf(const char *f, ...)
{
	char m[MESSAGE_LEN];
	va_list ap;

	va_start(ap, f);
	vsnprintf(m, sizeof(m), f, ap);
	va_end(ap);

	send(1, (uint8_t *) m);
}

	int
waitintr(int i)
{
	return -1;
}

	uint32_t
intcopylittle32(uint8_t *s)
{
	return 0;
}

	void
mmcreset(uint32_t line)
{
	int i;

	regs->stat = 0xffffffff;
	regs->sysctl |= line;

	i = 100;
	while (!(regs->sysctl & line) && i-- > 0)
		;

	if (i == 0) {
		printf("%s reset timeout waiting for set\n", name);
	}

	i = 10000;
	while ((regs->sysctl & line) && i-- > 0)
		;

	if (i == 0) {
		printf("%s reset timeout waiting for clear\n", name);
	}
}

	bool
mmchssendcmd(uint32_t cmd, uint32_t arg)
{
	uint32_t stat;

	if (regs->stat != 0) {
		printf("%s stat bad for cmd, 0b%b\n",
				name, regs->stat);

		return false;
	}

	regs->arg = arg;
	regs->cmd = cmd;

	if (waitintr(intr) != OK) {
		return false;
	}

	stat = regs->stat;

	regs->stat = MMCHS_SD_STAT_CC
		| MMCHS_SD_STAT_CIE
		| MMCHS_SD_STAT_CCRC
		| MMCHS_SD_STAT_CTO;

	if (stat & MMCHS_SD_STAT_ERRI) {
		mmcreset(MMCHS_SD_SYSCTL_SRC);
		return false;
	} else {
		return true;
	}
}

	bool
mmchssendappcmd(uint32_t cmd, uint32_t arg)
{
	uint32_t acmd;

	acmd = MMCHS_SD_CMD_INDEX_CMD(MMC_APP_CMD)
		| MMCHS_SD_CMD_RSP_TYPE_48B;

	if (!mmchssendcmd(acmd, MMC_ARG_RCA(rca))) {
		return false;
	}

	return mmchssendcmd(cmd, arg);
}

	bool
mmchssoftreset(void)
{
	int i = 100;

	regs->sysconfig |= MMCHS_SD_SYSCONFIG_SOFTRESET;

	while (!(regs->sysstatus & MMCHS_SD_SYSSTATUS_RESETDONE)) {
		yield();
		if (i-- < 0) {
			printf("%s: Failed to reset controller!\n", name);
			return false;
		}
	}

	return true;
}

	static bool
mmchsinit(void)
{
	int i;

	/* 1-bit mode */
	regs->con &= ~MMCHS_SD_CON_DW8;

	/* set to 3.0V */
	regs->hctl = MMCHS_SD_HCTL_SDVS_VS30;
	regs->capa |= MMCHS_SD_CAPA_VS18 | MMCHS_SD_CAPA_VS30;

	/* Power on card */
	regs->hctl |= MMCHS_SD_HCTL_SDBP;

	i = 100;
	while (!(regs->hctl & MMCHS_SD_HCTL_SDBP)) {
		yield();
		if (i-- < 0) {
			printf("%s: failed to power on card!\n", name);
			return false;
		}
	}

	/* enable internal clock and clock to card */
	regs->sysctl |= MMCHS_SD_SYSCTL_ICE;

	/* set clock frequence to 400kHz */
	regs->sysctl &= ~MMCHS_SD_SYSCTL_CLKD;
	regs->sysctl |= 0xf0 << 6;

	/* Set data and busy time out */
	regs->sysctl &= ~MMCHS_SD_SYSCTL_DTO;
	regs->sysctl |= MMCHS_SD_SYSCTL_DTO_2POW27;

	/* enable clock */
	regs->sysctl |= MMCHS_SD_SYSCTL_CEN;

	i = 100;
	do {
		yield();
		i--;
	} while (i > 0 && !(regs->sysctl & MMCHS_SD_SYSCTL_ICS));

	if (!i) {
		printf("%s: mmchs clock not stable\n", name);
		return false;
	}

	/* Enable interrupts */
	regs->ie = 0
		| MMCHS_SD_STAT_DCRC
		| MMCHS_SD_STAT_DTO
		| MMCHS_SD_STAT_DEB
		| MMCHS_SD_STAT_CIE
		| MMCHS_SD_STAT_CCRC
		| MMCHS_SD_STAT_CTO
		| MMCHS_SD_IE_CC
		| MMCHS_SD_IE_TC
		| MMCHS_SD_IE_BRR
		| MMCHS_SD_IE_BWR;

	/* enable send interrupts to intc */
	regs->ise = 0
		| MMCHS_SD_STAT_DCRC
		| MMCHS_SD_STAT_DTO
		| MMCHS_SD_STAT_DEB
		| MMCHS_SD_STAT_CIE
		| MMCHS_SD_STAT_CCRC
		| MMCHS_SD_STAT_CTO
		| MMCHS_SD_IE_CC
		| MMCHS_SD_IE_TC
		| MMCHS_SD_IE_BRR
		| MMCHS_SD_IE_BWR;

	regs->blk = 512;

	return true;
}

	static bool
mmchsinitstream(void)
{
	/* send init signal */
	regs->con |= MMCHS_SD_CON_INIT;

	mmchssendcmd(0, 0);

	/* remove init */
	regs->con &= ~MMCHS_SD_CON_INIT;

	return true;
}

	bool
cardgotoidle(void)
{
	uint32_t cmd;

	cmd = MMCHS_SD_CMD_INDEX_CMD(MMC_GO_IDLE_STATE)
		| MMCHS_SD_CMD_RSP_TYPE_NO_RESP;

	return mmchssendcmd(cmd, 0);
}

	bool
cardidentification(void)
{
	uint32_t cmd, arg;

	cmd = MMCHS_SD_CMD_INDEX_CMD(SD_SEND_IF_COND)
		| MMCHS_SD_CMD_RSP_TYPE_48B;

	arg = MMCHS_SD_ARG_CMD8_VHS | MMCHS_SD_ARG_CMD8_CHECK_PATTERN;

	if (!mmchssendcmd(cmd, arg)) {
		return false;
	}

	if (regs->rsp10 != arg) {
		printf("%s check pattern check failed 0x%h\n",
				name, regs->rsp10);
		return false;
	} else {
		return true;
	}
}

	bool
cardqueryvolttype(void)
{
	uint32_t cmd, arg;

	cmd = MMCHS_SD_CMD_INDEX_CMD(SD_APP_OP_COND)
		| MMCHS_SD_CMD_RSP_TYPE_48B;

	arg =
		MMC_OCR_3_3V_3_4V | MMC_OCR_3_2V_3_3V | MMC_OCR_3_1V_3_2V |
		MMC_OCR_3_0V_3_1V | MMC_OCR_2_9V_3_0V | MMC_OCR_2_8V_2_9V |
		MMC_OCR_2_7V_2_8V | MMC_OCR_HCS;

	if (mmchssendappcmd(cmd, arg)) {
		type = MMC_SD;

		while (!(regs->rsp10 & MMC_OCR_MEM_READY)) {
			if (!mmchssendappcmd(cmd, arg)) {
				return false;
			}
		}
	} else {
		type = MMC_EMMC;

		mmcreset(MMCHS_SD_SYSCTL_SRC);

		cmd = MMCHS_SD_CMD_INDEX_CMD(MMC_SEND_OP_COND)
			| MMCHS_SD_CMD_RSP_TYPE_48B;

		do {
			if (!mmchssendcmd(cmd, arg)) {
				return false;
			}
		} while (!(regs->rsp10 & MMC_OCR_MEM_READY));
	}

	return true;
}

	bool
cardidentify(void)
{
	uint32_t cmd;

	cmd = MMCHS_SD_CMD_INDEX_CMD(MMC_ALL_SEND_CID)
		| MMCHS_SD_CMD_RSP_TYPE_136B;

	if (!mmchssendcmd(cmd, 0)) {
		printf("%s send cid failed\n", name);
		return false;
	}

	cmd = MMCHS_SD_CMD_INDEX_CMD(MMC_SET_RELATIVE_ADDR)
		| MMCHS_SD_CMD_RSP_TYPE_48B
		| MMCHS_SD_CMD_CICE_ENABLE
		| MMCHS_SD_CMD_CCCE_ENABLE;

	if (!mmchssendcmd(cmd, 2)) {
		printf("%s set relative addr failed\n", name);
		return false;
	}

	rca = regs->rsp10 >> 16;

	return true;
}

	bool
cardcsd(void)
{
	uint32_t cmd;

	cmd = MMCHS_SD_CMD_INDEX_CMD(MMC_SEND_CSD)
		| MMCHS_SD_CMD_RSP_TYPE_136B;

	if (!mmchssendcmd(cmd, MMC_ARG_RCA(rca))) {
		return false;
	}

	csd[0] = regs->rsp10;
	csd[1] = regs->rsp32;
	csd[2] = regs->rsp54;
	csd[3] = regs->rsp76;

	return true;
}

	bool
cardselect(void)
{
	uint32_t cmd;

	cmd = MMCHS_SD_CMD_INDEX_CMD(MMC_SELECT_CARD)
		| MMCHS_SD_CMD_RSP_TYPE_48B
		| MMCHS_SD_CMD_CICE_ENABLE
		| MMCHS_SD_CMD_CCCE_ENABLE;

	return mmchssendcmd(cmd, MMC_ARG_RCA(rca));
}

	bool
mmchsreaddata(uint32_t *buf, size_t l)
{
	uint32_t stat;

	if (waitintr(intr) != OK) {
		return false;
	}

	if (regs->stat & MMCHS_SD_STAT_BRR) {
		while (l > 0) {
			*buf++ = regs->data;
			l -= sizeof(uint32_t);
		}

		regs->stat = MMCHS_SD_STAT_BRR;
	}

	if (waitintr(intr) != OK) {
		return false;
	}

	stat = regs->stat;
	regs->stat = MMCHS_SD_STAT_TC
		| MMCHS_SD_STAT_DCRC
		| MMCHS_SD_STAT_DTO
		| MMCHS_SD_STAT_DEB;

	if (stat & MMCHS_SD_STAT_ERRI) {
		mmcreset(MMCHS_SD_SYSCTL_SRD);
		return false;
	} else {
		return true;
	}
}

	bool
mmchswritedata(uint32_t *buf, size_t l)
{
	uint32_t stat;

	if (waitintr(intr) != OK) {
		return false;
	}

	if (regs->stat & MMCHS_SD_STAT_BWR) {
		while (l > 0) {
			regs->data = *buf++;
			l -= sizeof(uint32_t);
		}

		regs->stat = MMCHS_SD_STAT_BWR;
	}

	if (waitintr(intr) != OK) {
		return false;
	}

	stat = regs->stat;
	regs->stat = MMCHS_SD_STAT_TC
		| MMCHS_SD_STAT_DCRC
		| MMCHS_SD_STAT_DTO
		| MMCHS_SD_STAT_DEB;

	if (stat & MMCHS_SD_STAT_ERRI) {
		printf("%s write data error waiting for tc\n", name);
		mmcreset(MMCHS_SD_SYSCTL_SRD);
		return false;
	} else {
		return true;
	}
}

	bool
readblock(uint32_t blk, uint8_t *buf)
{
	uint32_t cmd;

	cmd = MMCHS_SD_CMD_INDEX_CMD(MMC_READ_BLOCK_SINGLE)
		| MMCHS_SD_CMD_RSP_TYPE_48B
		| MMCHS_SD_CMD_DP_DATA
		| MMCHS_SD_CMD_MSBS_SINGLE
		| MMCHS_SD_CMD_DDIR_READ;

	if (!mmchssendcmd(cmd, blk)) {
		return false;
	} 

	return mmchsreaddata((uint32_t *) buf, 512);
}

	bool
writeblock(uint32_t blk, uint8_t *buf)
{
	uint32_t cmd;

	cmd = MMCHS_SD_CMD_INDEX_CMD(MMC_WRITE_BLOCK_SINGLE)
		| MMCHS_SD_CMD_RSP_TYPE_48B
		| MMCHS_SD_CMD_DP_DATA
		| MMCHS_SD_CMD_MSBS_SINGLE
		| MMCHS_SD_CMD_DDIR_WRITE;

	if (!mmchssendcmd(cmd, blk)) {
		regs->stat = MMCHS_SD_STAT_ERROR_MASK;
		return false;
	}

	return mmchswritedata((uint32_t *) buf, 512);
}

	static bool
cardinit(void)
{
	if (!cardgotoidle()) {
		printf("%s failed to goto idle state!\n", name);
		return false;
	}

	if (!cardidentification()) {
		mmcreset(MMCHS_SD_SYSCTL_SRC);
	}

	if (!cardqueryvolttype()) {
		printf("%s failed to query voltage and type!\n", name);
		return false;
	}

	if (!cardidentify()) {
		printf("%s identify card failed!\n", name);
		return false;
	}

	if (!cardcsd()) {
		printf("%s get card csd failed!\n", name);
		return false;
	}

	if (!cardselect()) {
		printf("%s select card failed!\n", name);
		return false;
	}

	return true;
}

	static int
setup(void)
{
	if (!mmchssoftreset()) {
		printf("%s failed to reset host!\n", name);
		return ERR;
	}

	if (!mmchsinit()) {
		printf("%s failed to init host!\n", name);
		return ERR;
	}

	if (!mmchsinitstream()) {
		printf("%s failed to init host stream!\n", name);
		return ERR;
	}

	if (!cardinit()) {
		printf("%s failed to init card!\n", name);
		return ERR;
	}

	if (type == MMC_EMMC) {
		if (!mmcinit()) {
			printf("%s failed to initialise\n", name);
			return ERR;
		}

		printf("emmc card on %s\n", name);
	} else if (type == MMC_SD) {
		if (!sdinit()) {
			printf("%s failed to initialise\n", name);
			return ERR;
		}

		printf("sd card on %s\n", name);
	} else {
		printf("%s is of unknown type.\n", name);
		return ERR;
	}

	return OK;
}

	static struct mmchs_regs *
get_regs(void)
{
	struct proc0_dev_req *req;
	struct proc0_dev_resp *resp;
	uint8_t m[MESSAGE_LEN];
	int r, f_id;

	req = (struct proc0_dev_req *) m;
	req->type = proc0_type_dev;
	req->method = proc0_dev_req_method_compat;

	strlcpy(req->kind.compatability, 
			"ti,omap4-hsmmc",
			sizeof(req->kind.compatability));

	if (send(0, m) != OK) {
		return nil;
	}

	do {
		r = recv(m);
		if (r < 0) {
			return nil;
		}
	} while (r != 0);

	resp = (struct proc0_dev_resp *) m;
	if (resp->nframes == 0) {
		return nil;
	}

	f_id = resp->frames[0];

	return frame_map_free(f_id, F_MAP_READ|F_MAP_WRITE);
}

	void
main(void)
{
	uint8_t m[MESSAGE_LEN];

	name = "mmchs";

	regs = get_regs();
	if (regs == nil) {
		return;
	}

	if (setup() != OK) {
		return;
	}

	while (true) {
		recv(m);
	}
}

