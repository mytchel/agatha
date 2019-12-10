/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * ARM PrimeCell MultiMedia Card Interface - PL180
 *
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Ulf Hansson <ulf.hansson@stericsson.com>
 * Author: Martin Lundholm <martin.xa.lundholm@stericsson.com>
 * Ported to drivers/mmc/ by: Matt Waddel <matt.waddel@linaro.org>
 */

/* Taken and modified from U-Boot.
	 Probably only works if U-Boot has already set it up for us */

#include <types.h>
#include <err.h>
#include <mach.h>
#include <sys.h>
#include <sysobj.h>
#include <c.h>
#include <mesg.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <log.h>
#include <arm/pl18x.h>
#include <sdmmc.h>

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
	volatile struct pl18x_regs *regs = mmc->base;
	uint32_t hoststatus, statusmask;

	statusmask = SDI_STA_CTIMEOUT | SDI_STA_CCRCFAIL;
	if ((cmd->resp_type & MMC_RSP_PRESENT)) {
		statusmask |= SDI_STA_CMDREND;
	} else {
		statusmask |= SDI_STA_CMDSENT;
	}

	uint8_t m[MESSAGE_LEN];
	int from;

	regs->mask[0] = statusmask;

	if (recv(mmc->irq_eid, &from, m) < 0) {
		log(LOG_WARNING, "error getting irq");
		return -1;
	}
	
	regs->mask[0] = 0;

	intr_ack(mmc->irq_cid);

	hoststatus = regs->status & statusmask;

	regs->clear = statusmask;
	if (hoststatus & SDI_STA_CTIMEOUT) {
		log(LOG_WARNING, "mmc cmd %i timed out", cmd->cmdidx);
		return -1;
	} else if ((hoststatus & SDI_STA_CCRCFAIL) &&
		   (cmd->resp_type & MMC_RSP_CRC)) {
		log(LOG_WARNING, "mmc cmd %i crc error", cmd->cmdidx);
		return -1;
	}

	if (cmd->resp_type & MMC_RSP_PRESENT) {
		cmd->response[0] = regs->response[0];
		cmd->response[1] = regs->response[1];
		cmd->response[2] = regs->response[2];
		cmd->response[3] = regs->response[3];
	}

	return 0;
}

static int 
read_bytes(volatile struct pl18x_regs *regs,
		uint32_t *dest, uint32_t blkcount, uint32_t blksize)
{
	uint64_t xfercount = blkcount * blksize;
	uint32_t *tempbuff = dest;
	uint32_t status, status_err;

	status = regs->status;
	status_err = status & (SDI_STA_DCRCFAIL | SDI_STA_DTIMEOUT |
			       SDI_STA_RXOVERR);
	while ((!status_err) && (xfercount >= sizeof(uint32_t))) {
		if (status & SDI_STA_RXDAVL) {
			*(tempbuff) = *regs->fifo;
			tempbuff++;
			xfercount -= sizeof(uint32_t);
		}
		status = regs->status;
		status_err = status & (SDI_STA_DCRCFAIL | SDI_STA_DTIMEOUT |
				       SDI_STA_RXOVERR);
	}

	status_err = status &
		(SDI_STA_DCRCFAIL | SDI_STA_DTIMEOUT | SDI_STA_DBCKEND |
		 SDI_STA_RXOVERR);
	while (!status_err) {
		status = regs->status;
		status_err = status &
			(SDI_STA_DCRCFAIL | SDI_STA_DTIMEOUT | SDI_STA_DBCKEND |
			 SDI_STA_RXOVERR);
	}

	if (status & SDI_STA_DTIMEOUT) {
		return ERR;
	} else if (status & SDI_STA_DCRCFAIL) {
		return ERR;
	} else if (status & SDI_STA_RXOVERR) {
		return ERR;
	}

	regs->clear = SDI_ICR_MASK;

	if (xfercount) {
		return ERR;
	}

	return 0;
}

static int 
write_bytes(volatile struct pl18x_regs *regs,
		const uint32_t *src, uint32_t blkcount, uint32_t blksize)
{
	const uint32_t *tempbuff = src;
	uint64_t xfercount = blkcount * blksize;
	uint32_t status, status_err;
	int i;

	status = regs->status;
	status_err = status & (SDI_STA_DCRCFAIL | SDI_STA_DTIMEOUT);
	while (!status_err && xfercount) {
		if (status & SDI_STA_TXFIFOBW) {
			if (xfercount >= SDI_FIFO_BURST_SIZE * sizeof(uint32_t)) {
				for (i = 0; i < SDI_FIFO_BURST_SIZE; i++)
					*regs->fifo = *(tempbuff + i);
				tempbuff += SDI_FIFO_BURST_SIZE;
				xfercount -= SDI_FIFO_BURST_SIZE * sizeof(uint32_t);
			} else {
				while (xfercount >= sizeof(uint32_t)) {
					*regs->fifo = *(tempbuff);
					tempbuff++;
					xfercount -= sizeof(uint32_t);
				}
			}
		}
		status = regs->status;
		status_err = status & (SDI_STA_DCRCFAIL | SDI_STA_DTIMEOUT);
	}

	status_err = status &
		(SDI_STA_DCRCFAIL | SDI_STA_DTIMEOUT | SDI_STA_DBCKEND);
	while (!status_err) {
		status = regs->status;
		status_err = status &
			(SDI_STA_DCRCFAIL | SDI_STA_DTIMEOUT | SDI_STA_DBCKEND);
	}

	if (status & SDI_STA_DTIMEOUT) {
		return ERR;
	} else if (status & SDI_STA_DCRCFAIL) {
		return ERR;
	}

	regs->clear = SDI_ICR_MASK;

	if (xfercount) {
		return ERR;
	}

	return 0;
}

	static int 
do_command(struct mmc *mmc, struct mmc_cmd *cmd, struct mmc_data *data)
{
	volatile struct pl18x_regs *regs = mmc->base;
	uint32_t data_ctrl = 0;
	uint32_t sdi_cmd = 0;
	uint32_t blksz = 0;
	uint32_t data_len;
	int result;

	sdi_cmd = ((cmd->cmdidx & SDI_CMD_CMDINDEX_MASK) | SDI_CMD_CPSMEN);

	if (cmd->resp_type) {
		sdi_cmd |= SDI_CMD_WAITRESP;
		if (cmd->resp_type & MMC_RSP_136)
			sdi_cmd |= SDI_CMD_LONGRESP;
	}

	if (data != nil) {
		data_len = (uint32_t) (data->blocks * data->blocksize);
		blksz = data->blocksize;
		data_ctrl |= (blksz << SDI_DCTRL_DBLOCKSIZE_V2_SHIFT);
		data_ctrl |= SDI_DCTRL_DTEN | SDI_DCTRL_BUSYMODE;

		if (data->flags & MMC_DATA_READ) {
			data_ctrl |= SDI_DCTRL_DTDIR_IN;
		}

		regs->datatimer = SDI_DTIMER_DEFAULT;
		regs->datalength = data_len;
		udelay(DATA_REG_DELAY);
		regs->datactrl = data_ctrl;
	}

	regs->argument = cmd->cmdarg;
	udelay(COMMAND_REG_DELAY);
	regs->command = sdi_cmd;
	result = wait_for_command_end(mmc, cmd);

	/* After CMD2 set RCA to a none zero value. */
	if ((result == 0) && (cmd->cmdidx == MMC_CMD_ALL_SEND_CID))
		mmc->rca = 10;

	/* After CMD3 open drain is switched off and push pull is used. */
	if ((result == 0) && (cmd->cmdidx == MMC_CMD_SET_RELATIVE_ADDR)) {
		uint32_t sdi_pwr = regs->power & ~SDI_PWR_OPD;
		regs->power = sdi_pwr;
	}

	if (data != nil) {
		if (data->flags & MMC_DATA_READ) {
			result = read_bytes(mmc->base, data->dest, data->blocks,
					data->blocksize);

		} else if (data->flags & MMC_DATA_WRITE) {
			result = write_bytes(mmc->base, data->src, data->blocks,
					data->blocksize);
		}
	}

	return result;
}

	static int
pl18x_set_ios(struct mmc *mmc)
{
	volatile struct pl18x_regs *regs = mmc->base;
	uint32_t sdi_clkcr;

	sdi_clkcr = regs->clock;

	sdi_clkcr &= ~(SDI_CLKCR_CLKDIV_MASK);
	sdi_clkcr |= 0;

	sdi_clkcr &= ~(SDI_CLKCR_WIDBUS_MASK);
	sdi_clkcr |= SDI_CLKCR_WIDBUS_1;

	log(LOG_INFO, "setting clock to 0x%x", sdi_clkcr);
	regs->clock = sdi_clkcr;
	udelay(CLK_CHANGE_DELAY);
	log(LOG_INFO, "clock now 0x%x", regs->clock);

	return 0;
}

	static int
pl18x_reset(struct mmc *mmc)
{
	volatile struct pl18x_regs *regs = mmc->base;

	regs->power = INIT_PWR;

	return 0;
}

	static int 
pl18x_init(volatile struct pl18x_regs *regs)
{
	regs->power = INIT_PWR;
	regs->clock = SDI_CLKCR_CLKDIV_INIT_V1 | SDI_CLKCR_CLKEN;

	udelay(CLK_CHANGE_DELAY);

	/* Disable mmc interrupts */
	regs->mask[0] = 0;

	return 0;
}

	void
main(void)
{
	struct mmc mmc = { 0 };

	char dev_name[] = "test_mmc";

	volatile struct pl18x_regs *regs;

	int mount_eid;
	int reg_cid;
	int irq_cid;

	union proc0_req prq;
	union proc0_rsp prp;
	int ret;

	log_init(dev_name);

	log(LOG_INFO, "pl18x get caps");

	mount_eid = kcap_alloc();
	reg_cid = kcap_alloc();
	irq_cid = kcap_alloc();

	if (mount_eid < 0 || reg_cid < 0 || irq_cid < 0) {
		exit(ERR);
	}

	log(LOG_INFO, "pl18x request mount");

	prq.get_resource.type = PROC0_get_resource;
	prq.get_resource.resource_type = RESOURCE_type_mount;

	mesg_cap(CID_PARENT, &prq, &prp, mount_eid);
	if (prp.get_resource.ret != OK) {
		exit(ERR);
	}

	log(LOG_INFO, "pl18x request regs");

	prq.get_resource.type = PROC0_get_resource;
	prq.get_resource.resource_type = RESOURCE_type_regs;

	mesg_cap(CID_PARENT, &prq, &prp, reg_cid);
	if (prp.get_resource.ret != OK) {
		exit(ERR);
	}

	log(LOG_INFO, "pl18x request int");

	prq.get_resource.type = PROC0_get_resource;
	prq.get_resource.resource_type = RESOURCE_type_int;

	mesg_cap(CID_PARENT, &prq, &prp, irq_cid);
	if (prp.get_resource.ret != OK) {
		exit(ERR);
	}

	log(LOG_INFO, "pl18x got mount 0x%x, reg 0x%x, irq 0x%x",
		mount_eid, reg_cid, irq_cid);

	log(LOG_INFO, "map reg\n");

	regs = frame_map_anywhere(reg_cid);
	if (regs == nil) {
		log(LOG_FATAL, "pl18x failed to map registers!");
		exit(ERR);
	}

	log(LOG_INFO, "pl18x init");

	ret = pl18x_init(regs);
	if (ret != OK) {
		log(LOG_FATAL, "pl18x init failed!");
		exit(ret);
	}

	log(LOG_INFO, "pl18x connect irq");

	int int_eid = kobj_alloc(OBJ_endpoint, 1);
	if (intr_connect(irq_cid, int_eid, 0x1) != OK) {
		exit(ERR);
	}

	mmc.base = regs;
	mmc.irq_eid = int_eid;
	mmc.irq_cid = irq_cid;
	mmc.name = dev_name;

	mmc.voltages = 0xff8080;

	mmc.command = &do_command;
	mmc.set_ios = &pl18x_set_ios;
	mmc.reset = &pl18x_reset;

	log(LOG_INFO, "mmc_start");
	ret = mmc_start(&mmc);
	log(LOG_WARNING, "mmc_start returned %i!", ret);
}

