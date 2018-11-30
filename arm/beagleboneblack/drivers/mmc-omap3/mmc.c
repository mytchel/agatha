#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <arm/omap3_mmc.h>
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
		pid = get_device_pid("serial0");
	}

	char s[MESSAGE_LEN] = "omap3_mmchs: ";
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

void
dump_regs(struct mmc *mmc)
{
	volatile uint32_t *regs = mmc->base;

	int i;
	for (i = 68; i < sizeof(struct omap3_mmchs_regs) / 4; i++)
		debug(mmc, "0x%x -> 0x%x\n", i * 4, regs[i]);
}

static void
udelay(size_t us)
{
	size_t j;

	while (us-- > 0)
		for (j = 0; j < 1000; j++)
			;
}

void
mmchs_reset_line(volatile struct omap3_mmchs_regs *regs, 
		uint32_t line)
{
	int i;

	regs->sysctl |= line;

	udelay(10);

	i = 10000;
	while ((regs->sysctl & line) && i-- > 0)
		;
}

static void
wait_for_intr(volatile struct omap3_mmchs_regs *regs,
		uint32_t mask)
{
	while (!(regs->stat & mask))
		;
}

static int 
do_command(struct mmc *mmc, struct mmc_cmd *cmd, struct mmc_data *data)
{
	volatile struct omap3_mmchs_regs *regs = mmc->base;
	uint32_t send, stat;
	size_t l;

	debug(mmc, "sending cmd 0x%x with arg 0x%x\n", cmd->cmdidx, cmd->cmdarg);
	send = MMCHS_SD_CMD_INDEX_CMD(cmd->cmdidx);

	if (cmd->resp_type) {
		if (cmd->resp_type & MMC_RSP_PRESENT) {
			if (cmd->resp_type & MMC_RSP_BUSY) {
				debug(mmc, "with busy\n");
				send |= MMCHS_SD_CMD_RSP_TYPE_48B_BUSY;
			} else if (cmd->resp_type & MMC_RSP_136) {
				debug(mmc, "with long\n");
				send |= MMCHS_SD_CMD_RSP_TYPE_136B;
			} else {
				debug(mmc, "with normal\n");
				send |= MMCHS_SD_CMD_RSP_TYPE_48B;
			}
		}

		if (cmd->resp_type & MMC_RSP_CRC) {
				debug(mmc, "with crc\n");
			send |= MMCHS_SD_CMD_CCCE_ENABLE;
		}

		if (cmd->resp_type & MMC_RSP_OPCODE) {
			debug(mmc, "with opcode\n");
			send |= MMCHS_SD_CMD_CICE_ENABLE;
		}
	}

	if (data != nil) {
		send |= MMCHS_SD_CMD_DP_DATA |
			MMCHS_SD_CMD_MSBS_SINGLE;
		debug(mmc, "with data\n");

		if (data->flags == MMC_DATA_READ) {
			debug(mmc, "with read\n");
			send |= MMCHS_SD_CMD_DDIR_READ;

		} else if (data->flags == MMC_DATA_WRITE) {
			debug(mmc, "with write\n");
			send |= MMCHS_SD_CMD_DDIR_WRITE;
		}
	}

	debug(mmc, "sending cmd 0x%x with resp of 0x%x as 0x%x\n", cmd->cmdidx, cmd->resp_type, send);
	debug(mmc, "stat before = 0x%x\n", regs->stat);

	regs->arg = cmd->cmdarg;
	regs->cmd = send;

	wait_for_intr(regs, MMCHS_SD_STAT_CC | MMCHS_SD_STAT_ERRI);

	stat = regs->stat;

	debug(mmc, "got stat of 0x%x\n", stat);

	regs->stat = MMCHS_SD_STAT_CC |
			MMCHS_SD_STAT_CIE |
			MMCHS_SD_STAT_CCRC |
			MMCHS_SD_STAT_CTO;

	if (stat & MMCHS_SD_STAT_CTO) {
		debug(mmc, "reset src line\n");
		mmchs_reset_line(regs, MMCHS_SD_SYSCTL_SRC);
	}

	if (stat & MMCHS_SD_STAT_DTO) {
		debug(mmc, "reset src srd\n");
		mmchs_reset_line(regs, MMCHS_SD_SYSCTL_SRD);
	}

	debug(mmc, "stat now 0x%x\n", regs->stat);
	if (stat & MMCHS_SD_STAT_ERRI) {
		debug(mmc, "command failed stat = 0x%x\n", stat);
		return ERR;
	}

	if (cmd->resp_type & MMC_RSP_PRESENT) {
		cmd->response[0] = regs->rsp10;
		if (cmd->resp_type & MMC_RSP_136) {
			cmd->response[1] = regs->rsp32;
			cmd->response[2] = regs->rsp54;
			cmd->response[3] = regs->rsp76;
		}
	}

	if (data != nil) {
		/* TODO: error handling */

		debug(mmc, "handling data\n");

		if (data->flags == MMC_DATA_READ) {
			wait_for_intr(regs, MMCHS_SD_STAT_ERRI | MMCHS_SD_STAT_BRR);
			if (stat & MMCHS_SD_STAT_ERRI) {
				return ERR;
			}

			for (l = 0; l * sizeof(uint32_t) < data->blocks * data->blocksize; l++) {
				data->dest[l] = regs->data;	
			}

			regs->stat = MMCHS_SD_STAT_BRR;

		} else if (data->flags == MMC_DATA_WRITE && (stat & MMCHS_SD_STAT_BWR)) {
			wait_for_intr(regs, MMCHS_SD_STAT_ERRI | MMCHS_SD_STAT_BWR);
			if (stat & MMCHS_SD_STAT_ERRI) {
				return ERR;
			}

			for (l = 0; l * sizeof(uint32_t) < data->blocks * data->blocksize; l++) {
				regs->data = data->src[l];
			}

			regs->stat = MMCHS_SD_STAT_BWR;

		} else {
			return ERR;
		}

		wait_for_intr(regs, MMCHS_SD_STAT_ERRI | MMCHS_SD_STAT_TC);
		if (stat & MMCHS_SD_STAT_ERRI) {
			return ERR;
		}
	}

	return OK;
}

	static int
mmchs_set_ios(struct mmc *mmc)
{
	debug(mmc, "set ios\n");

	return OK;
}

	static int
mmchs_reset(struct mmc *mmc)
{
	volatile struct omap3_mmchs_regs *regs = mmc->base;
	int i = 100;

	debug(mmc, "reset\n");

	regs->sysconfig |= MMCHS_SD_SYSCONFIG_SOFTRESET;

	while (!(regs->sysstatus & MMCHS_SD_SYSSTATUS_RESETDONE)) {
		udelay(1000);
		if (i-- < 0) {
			debug(mmc, "reset failed!\n");
			return ERR;
		}
	}

	return OK;
}

	static int 
mmchs_init(struct mmc *mmc)
{
	volatile struct omap3_mmchs_regs *regs = mmc->base;
	int i;

	if (mmchs_reset(mmc) != 0) {
		return ERR;
	}

	regs->con &= ~MMCHS_SD_CON_DW8;
	
	regs->hctl = MMCHS_SD_HCTL_SDVS_VS30;
	regs->capa |= MMCHS_SD_CAPA_VS18 | MMCHS_SD_CAPA_VS30;

	regs->hctl |= MMCHS_SD_HCTL_SDBP;

	i = 100;
	while (!(regs->hctl & MMCHS_SD_HCTL_SDBP)) {
		udelay(1000);
		if (i-- < 0) {
			debug(mmc, "failed to power on card!\n");
			return ERR;
		}
	}

	regs->sysctl |= MMCHS_SD_SYSCTL_ICE;

	regs->sysctl &= ~MMCHS_SD_SYSCTL_CLKD;
	regs->sysctl |= 0xf0 << 6;

	regs->sysctl &= ~MMCHS_SD_SYSCTL_DTO;
	regs->sysctl |= MMCHS_SD_SYSCTL_DTO_2POW27;

	regs->sysctl |= MMCHS_SD_SYSCTL_CEN;

	i = 100;
	while (!(regs->sysctl & MMCHS_SD_SYSCTL_ICS)) {
		udelay(100);
		if (i-- < 0) {
			debug(mmc, "clock not stable!\n");
			return ERR;
		}
	}

	regs->ie = 
		MMCHS_SD_STAT_DCRC |
		MMCHS_SD_STAT_DTO |
		MMCHS_SD_STAT_DEB |
		MMCHS_SD_STAT_CIE |
		MMCHS_SD_STAT_CCRC |
		MMCHS_SD_STAT_CTO |
		MMCHS_SD_IE_CC |
		MMCHS_SD_IE_TC |
		MMCHS_SD_IE_BRR |
		MMCHS_SD_IE_BWR;

	regs->ise =  
		MMCHS_SD_STAT_DCRC |
		MMCHS_SD_STAT_DTO |
		MMCHS_SD_STAT_DEB |
		MMCHS_SD_STAT_CIE |
		MMCHS_SD_STAT_CCRC |
		MMCHS_SD_STAT_CTO |
		MMCHS_SD_IE_CC |
		MMCHS_SD_IE_TC |
		MMCHS_SD_IE_BRR |
		MMCHS_SD_IE_BWR;

	regs->blk = 512;

	regs->con |= MMCHS_SD_CON_INIT;	

	regs->cmd = 0;	

	udelay(1000);

	regs->stat = 1;

	regs->con &= ~MMCHS_SD_CON_INIT;	

	regs->stat = 0xffffffff;

	return OK;
}

	void
main(void)
{
	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];
	char name[MESSAGE_LEN];

	volatile struct omap3_mmchs_regs *regs;
	size_t regs_pa, regs_len;
	struct mmc mmc;
	int ret;

	recv(0, init_m);

	regs_pa = init_m[0];
	regs_len = init_m[1];

	recv(0, name);

	regs = map_addr(regs_pa, regs_len, MAP_DEV|MAP_RW);
	if (regs == nil) {
		debug(nil, "mmc-aml failed to map registers!\n");
		exit();
	}

	mmc.base = regs;
	mmc.name = name;

	mmc.voltages = OCR_VOLTAGE_MASK;

	mmc.command = &do_command;
	mmc.set_ios = &mmchs_set_ios;
	mmc.reset = &mmchs_reset;

	mmc.debug = &debug;

	debug(&mmc, "mmchs_init\n");
	ret = mmchs_init(&mmc);
	if (ret != OK) {
		debug(&mmc, "init failed!\n");
		exit();
	}

	debug(&mmc, "mmc_start\n");
	ret = mmc_start(&mmc);
	debug(&mmc, "mmc_start returned %i\n", ret);
}

