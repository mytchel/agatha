#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <arm/aml_meson8_mmc.h>
#include <sdmmc.h>
#include <dev_reg.h>

int
get_device_pid(char *name)
{
	union dev_reg_req rq;
	union dev_reg_rsp rp;

	rq.find.type = DEV_REG_find;
	snprintf(rq.find.name, sizeof(rq.find.name),
			"%s", name);

	if (send(DEV_REG_PID, &rq) != OK) {
		return ERR;
	} else if (recv(DEV_REG_PID, &rp) != DEV_REG_PID) {
		return ERR;
	} else if (rp.find.ret != OK) {
		return ERR;
	} else {
		return rp.find.pid;
	}
}

void
debug(struct mmc *mmc, char *fmt, ...)
{
	static int pid = -1;

	while (pid < 0) {
		pid = get_device_pid("uart0-ao");
	}

	char s[MESSAGE_LEN] = "aml_meson8_mmc:0: ";
	va_list ap;

	if (mmc != nil) {
		snprintf(s, sizeof(s), "%s: ", mmc->name);
	}

	va_start(ap, fmt);
	vsnprintf(s + strlen(s), 
			sizeof(s) - strlen(s), 
			fmt, ap);
	va_end(ap);

	send(pid, (uint8_t *) s);
	recv(pid, (uint8_t *) s);
}

static void
udelay(size_t us)
{
	size_t j;

	while (us-- > 0)
		for (j = 0; j < 1000; j++)
			;
}

static int 
wait_for_command_end(struct mmc *mmc,
		struct mmc_cmd *cmd)
{
	volatile struct aml_meson8_mmc_regs *regs = mmc->base;
	uint32_t hoststatus, statusmask;

	debug(mmc, "wait for command end\n");

	statusmask = ISTA_CTIMEOUT | ISTA_CCRCFAIL;
	if ((cmd->resp_type & MMC_RSP_PRESENT)) {
		statusmask |= ISTA_COK;
	} else {
		statusmask |= ISTA_DAT1_INT | ISTA_DAT1_INT_A;
	}

	do {
		hoststatus = regs->ista & statusmask;
		debug(mmc, "stat = 0x%x, mask = 0x%x, host status = 0x%x\n", regs->ista, statusmask, hoststatus);
	} while (!hoststatus);

	debug(mmc, "check\n");

	regs->ista = ISTA_ICR_MASK;
	if (hoststatus & ISTA_CTIMEOUT) {
		debug(mmc, "mmc cmd %i timed out\n", cmd->cmdidx);
		return -1;
	} else if ((hoststatus & ISTA_CCRCFAIL) &&
		   (cmd->resp_type & MMC_RSP_CRC)) {
		debug(mmc, "mmc cmd %i crc error\n", cmd->cmdidx);
		return -1;
	}

	debug(mmc, "ista now 0x%x\n", regs->ista);
	debug(mmc, "pdma rdresp = 0x%x\n", (regs->pdma >> 1) & 0x7);

	debug(mmc, "arg0 0 = 0x%x\n", regs->argu);
	debug(mmc, "arg0 1 = 0x%x\n", regs->argu);
	debug(mmc, "arg0 2 = 0x%x\n", regs->argu);
	debug(mmc, "arg0 3 = 0x%x\n", regs->argu);

	if (cmd->resp_type & MMC_RSP_PRESENT) {
		cmd->response[0] = 0;
		cmd->response[1] = 0;
		cmd->response[2] = 0;
		cmd->response[3] = 0;
	}

	return 0;
}
	static int 
read_bytes(volatile struct aml_meson8_mmc_regs *regs,
		uint32_t *dest, uint32_t blkcount, uint32_t blksize)
{
	uint64_t xfercount = blkcount * blksize;
	uint32_t status, status_err;

	debug(nil, "read bytes\n");

	status = regs->ista;
	status_err = status & (ISTA_DCRCFAIL | ISTA_DTIMEOUT | ISTA_DTOK);
	while ((!status_err) && (xfercount > 0)) {
		if (status & ISTA_DOK) {
			*dest++ = regs->data;
			xfercount -= sizeof(u32);
		}
		status = regs->ista;
		status_err = status & (ISTA_DCRCFAIL | ISTA_DTIMEOUT | ISTA_DTOK);
	}

	status_err = status & (ISTA_DCRCFAIL | ISTA_DTIMEOUT);
	while (!status_err) {
		status = regs->ista;
		status_err = status & (ISTA_DCRCFAIL | ISTA_DTIMEOUT);
	}

	if (status & ISTA_DTIMEOUT) {
		return ERR;
	} else if (status & ISTA_DCRCFAIL) {
		return ERR;
	}

	regs->ista = ISTA_ICR_MASK;

	if (xfercount) {
		return ERR;
	}

	return 0;
}

static int 
do_command(struct mmc *mmc, struct mmc_cmd *cmd, struct mmc_data *data)
{
	volatile struct aml_meson8_mmc_regs *regs = mmc->base;
	uint32_t send = 0;
	int result;

	debug(mmc, "do command\n");

	send = cmd->cmdidx & CMD_CMDINDEX_MASK;

	if (cmd->resp_type) {
		debug(mmc, "has response\n");
		send |= CMD_HAS_RESP;
		if (cmd->resp_type & MMC_RSP_136)
			send |= CMD_LONGRESP;
	}

	regs->argu = cmd->cmdarg;
	udelay(COMMAND_REG_DELAY);
	debug(mmc, "sending 0x%x\n", send);

	regs->send = send;
	result = wait_for_command_end(mmc, cmd);

	/* After CMD2 set RCA to a none zero value. */
	if ((result == 0) && (cmd->cmdidx == MMC_CMD_ALL_SEND_CID))
		mmc->rca = 10;

	if (data != nil) {
		if (data->flags & MMC_DATA_READ) {
			result = read_bytes(regs, data->dest, 
					data->blocks, data->blocksize);

		} else if (data->flags & MMC_DATA_WRITE) {
			return ERR;
		}
	}

	return result;
}

	static int
aml_meson8_mmc_set_ios(struct mmc *mmc)
{
	debug(mmc, "set ios\n");

	/*
		 uint32_t sdi_clkcr;
		 sdi_clkcr = regs->clock;

		 sdi_clkcr &= ~(SDI_CLKCR_CLKDIV_MASK);
		 sdi_clkcr |= 0;

		 sdi_clkcr &= ~(SDI_CLKCR_WIDBUS_MASK);
		 sdi_clkcr |= SDI_CLKCR_WIDBUS_1;

		 debug(mmc, "setting clock to 0x%x\n", sdi_clkcr);
		 regs->clock = sdi_clkcr;
		 udelay(CLK_CHANGE_DELAY);
		 debug(mmc, "clock now 0x%x\n", regs->clock);
	 */

	return 0;
}

	static int
aml_meson8_mmc_reset(struct mmc *mmc)
{
	volatile struct aml_meson8_mmc_regs *regs = mmc->base;

	debug(mmc, "reset\n");

	regs->srst = 0x1f;

	return 0;
}

	static int 
aml_meson8_mmc_init(volatile struct aml_meson8_mmc_regs *regs)
{
	uint32_t t;

	t = regs->stat;
	debug(nil, "mmc stat 0x%x\n", t);
	t = regs->misc;
	debug(nil, "mmc misc 0x%x\n", t);

	debug(nil, "mmc cntl 0x%x\n", regs->cntl);

	regs->cntl = 0 | (64<<13) | (8<<20);
	debug(nil, "mmc cntl 0x%x\n", regs->cntl);

	debug(nil, "mmc pdma 0x%x\n", regs->pdma);
	regs->pdma = 0;
	debug(nil, "mmc pdma 0x%x\n", regs->pdma);

	debug(nil, "mmc clkc 0x%x\n", regs->clkc);
	regs->clkc = 0;
	regs->clkc = 0 | (0<<16) | (1<<19) | (1<<23);
	debug(nil, "mmc clkc 0x%x\n", regs->clkc);

	debug(nil, "stat = 0x%x\n", regs->stat);

	/* Disable interrupts */
	regs->ictl = 0;

	/*
		 regs->power = INIT_PWR;
		 regs->clock = SDI_CLKCR_CLKDIV_INIT_V1 | SDI_CLKCR_CLKEN;

		 udelay(CLK_CHANGE_DELAY);

	 */
	/* Disable mmc interrupts */
	/*
		 sdi_u32 = regs->mask[0] & ~SDI_MASK0_MASK;
		 regs->mask[0] = sdi_u32;
	 */

	return 0;
}

	void
main(void)
{
	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];
	char name[MESSAGE_LEN];

	volatile struct aml_meson8_mmc_regs *regs;
	size_t regs_pa, regs_len, off;
	struct mmc mmc;
	int ret;

	recv(0, init_m);

	regs_pa = init_m[0];
	regs_len = init_m[1];

	recv(0, name);

	off = regs_pa - PAGE_ALIGN_DN(regs_pa);
	regs_pa = PAGE_ALIGN_DN(regs_pa);
	regs_len = PAGE_ALIGN(regs_len);

	regs = map_addr(regs_pa, regs_len, MAP_DEV|MAP_RW);
	if (regs == nil) {
		debug(nil, "mmc-aml failed to map registers!\n");
		exit();
	}

	regs = (void *) ((size_t) regs + off);

	ret = aml_meson8_mmc_init(regs);
	if (ret != OK) {
		debug(nil, "init failed!\n");
		exit();
	}

	mmc.base = regs;
	mmc.name = name;

	mmc.voltages = 0xff8080;

	mmc.command = &do_command;
	mmc.set_ios = &aml_meson8_mmc_set_ios;
	mmc.reset = &aml_meson8_mmc_reset;

	mmc.debug = &debug;

	ret = mmc_start(&mmc);
	debug(nil, "mmc_start returned %i\n", ret);
}

