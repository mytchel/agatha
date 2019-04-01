#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <log.h>
#include <arm/omap3_mmc.h>
#include <sdmmc.h>
#include <dev_reg.h>

static void intr_handler(int irqn, void *arg)
{
	struct mmc *mmc = arg;
	volatile struct omap3_mmchs_regs *regs;
	uint8_t m[MESSAGE_LEN];

	regs = mmc->base;
	regs->ie = 0;
	regs->ise = 0;

	send(pid(), m);
	intr_exit();
}

	static int
wait_for_intr(volatile struct omap3_mmchs_regs *regs,
		uint32_t mask)
{
	uint8_t m[MESSAGE_LEN];

	regs->ie = mask	|
		MMCHS_SD_STAT_DCRC |
		MMCHS_SD_STAT_DTO |
		MMCHS_SD_STAT_DEB |
		MMCHS_SD_STAT_CIE |
		MMCHS_SD_STAT_CCRC |
		MMCHS_SD_STAT_CTO;

	return recv(pid(), m);
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

	static int 
do_command(struct mmc *mmc, struct mmc_cmd *cmd, struct mmc_data *data)
{
	volatile struct omap3_mmchs_regs *regs = mmc->base;
	uint32_t send, stat;
	size_t l;

	log(LOG_INFO, "sending cmd 0x%x with arg 0x%x", cmd->cmdidx, cmd->cmdarg);
	send = MMCHS_SD_CMD_INDEX_CMD(cmd->cmdidx);

	if (cmd->resp_type) {
		if (cmd->resp_type & MMC_RSP_PRESENT) {
			if (cmd->resp_type & MMC_RSP_BUSY) {
				send |= MMCHS_SD_CMD_RSP_TYPE_48B_BUSY;
			} else if (cmd->resp_type & MMC_RSP_136) {
				send |= MMCHS_SD_CMD_RSP_TYPE_136B;
			} else {
				send |= MMCHS_SD_CMD_RSP_TYPE_48B;
			}
		}

		if (cmd->resp_type & MMC_RSP_CRC) {
			send |= MMCHS_SD_CMD_CCCE_ENABLE;
		}

		if (cmd->resp_type & MMC_RSP_OPCODE) {
			send |= MMCHS_SD_CMD_CICE_ENABLE;
		}
	}

	if (data != nil) {
		send |= MMCHS_SD_CMD_DP_DATA |
			MMCHS_SD_CMD_MSBS_SINGLE;

		if (data->flags == MMC_DATA_READ) {
			send |= MMCHS_SD_CMD_DDIR_READ;

		} else if (data->flags == MMC_DATA_WRITE) {
			send |= MMCHS_SD_CMD_DDIR_WRITE;
		}
	}

	regs->arg = cmd->cmdarg;
	regs->cmd = send;

	wait_for_intr(regs, MMCHS_SD_STAT_CC);

	stat = regs->stat;

	regs->stat = MMCHS_SD_STAT_CC |
		MMCHS_SD_STAT_CIE |
		MMCHS_SD_STAT_CCRC |
		MMCHS_SD_STAT_CTO |
		MMCHS_SD_STAT_DTO;

	if (stat & MMCHS_SD_STAT_CTO) {
		mmchs_reset_line(regs, MMCHS_SD_SYSCTL_SRC);
	}

	if (stat & MMCHS_SD_STAT_DTO) {
		mmchs_reset_line(regs, MMCHS_SD_SYSCTL_SRD);
	}

	if (stat & MMCHS_SD_STAT_ERRI) {
		log(LOG_INFO, "command failed stat = 0x%x", stat);
		return ERR;
	}

	if (cmd->resp_type & MMC_RSP_PRESENT) {
		if (cmd->resp_type & MMC_RSP_136) {
			cmd->response[3] = regs->rsp10;
			cmd->response[2] = regs->rsp32;
			cmd->response[1] = regs->rsp54;
			cmd->response[0] = regs->rsp76;
		} else {
			cmd->response[0] = regs->rsp10;
		}
	}

	if (data != nil) {
		/* TODO: error handling */

		if (data->flags == MMC_DATA_READ) {
			wait_for_intr(regs, MMCHS_SD_STAT_BRR);
			stat = regs->stat;
			if (stat & MMCHS_SD_STAT_ERRI) {
				return ERR;
			}

			for (l = 0; l * sizeof(uint32_t) < data->blocks * data->blocksize; l++) {
				data->dest[l] = regs->data;	
			}

			regs->stat = MMCHS_SD_STAT_BRR;

		} else if (data->flags == MMC_DATA_WRITE && (stat & MMCHS_SD_STAT_BWR)) {
			wait_for_intr(regs, MMCHS_SD_STAT_BWR);
			stat = regs->stat;
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

		wait_for_intr(regs, MMCHS_SD_STAT_TC);
		stat = regs->stat;
		if (stat & MMCHS_SD_STAT_ERRI) {
			return ERR;
		}
	}

	return OK;
}

	static int
mmchs_set_ios(struct mmc *mmc)
{
	log(LOG_INFO, "set ios");

	return OK;
}

	static int
mmchs_reset(struct mmc *mmc)
{
	volatile struct omap3_mmchs_regs *regs = mmc->base;
	int i = 100;

	log(LOG_INFO, "reset");

	regs->sysconfig |= MMCHS_SD_SYSCONFIG_SOFTRESET;

	while (!(regs->sysstatus & MMCHS_SD_SYSSTATUS_RESETDONE)) {
		udelay(1000);
		if (i-- < 0) {
			log(LOG_WARNING, "reset failed!");
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
			log(LOG_WARNING, "failed to power on card!");
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
			log(LOG_INFO, "clock not stable!");
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
	char dev_name[MESSAGE_LEN];

	static uint8_t intr_stack[128]__attribute__((__aligned__(4)));
	struct mmc mmc = { 0 };

	volatile struct omap3_mmchs_regs *regs;
	size_t regs_pa, regs_len, irqn;
	union proc0_req rq;
	union proc0_rsp rp;
	int ret;

	recv(0, init_m);

	regs_pa = init_m[0];
	regs_len = init_m[1];
	irqn = init_m[2];

	recv(0, dev_name);

	log_init(dev_name);

	regs = map_addr(regs_pa, regs_len, MAP_DEV|MAP_RW);
	if (regs == nil) {
		log(LOG_FATAL, "mmc-omap3 failed to map registers!");
		exit();
	}

	mmc.base = regs;
	mmc.irqn = irqn;
	mmc.name = dev_name;

	mmc.voltages = OCR_VOLTAGE_MASK;

	mmc.command = &do_command;
	mmc.set_ios = &mmchs_set_ios;
	mmc.reset = &mmchs_reset;

	rq.irq_reg.type = PROC0_irq_reg;
	rq.irq_reg.irqn = irqn; 
	rq.irq_reg.func = &intr_handler;
	rq.irq_reg.arg = &mmc;
	rq.irq_reg.sp = &intr_stack[sizeof(intr_stack) - 4];

	if (mesg(PROC0_PID, &rq, &rp) != OK || rp.irq_reg.ret != OK) {
		log(LOG_FATAL, "failed to register interrupt %i : %i", 
				irqn, rp.irq_reg.ret);
		exit();
	}

	log(LOG_INFO, "on pid %i mapped 0x%x -> 0x%x with irq %i",
			pid(), regs_pa, regs, irqn);

	ret = mmchs_init(&mmc);
	if (ret != OK) {
		log(LOG_FATAL, "init failed!");
		exit();
	}

	log(LOG_INFO, "mmc_start");
	ret = mmc_start(&mmc);
	log(LOG_INFO, "mmc_start returned %i", ret);
}

