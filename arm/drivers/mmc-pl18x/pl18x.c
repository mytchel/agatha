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

/* Taken and modified from U-Boot */

#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <m.h>
#include <stdarg.h>
#include <string.h>

#include <arm/pl18x.h>
#include <mmc.h>

extern uint32_t *_data_end;

void
debug(struct mmc *mmc, char *fmt, ...)
{
	char s[MESSAGE_LEN] = "MMC0: ";
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(s + 6, sizeof(s) - 6, fmt, ap);
	va_end(ap);

	send(1, (uint8_t *) s);
	recv(1, (uint8_t *) s);
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
	volatile struct pl18x_regs *regs = mmc->base;
	u32 hoststatus, statusmask;

	statusmask = SDI_STA_CTIMEOUT | SDI_STA_CCRCFAIL;
	if ((cmd->resp_type & MMC_RSP_PRESENT)) {
		statusmask |= SDI_STA_CMDREND;
	} else {
		statusmask |= SDI_STA_CMDSENT;
	}

	do {
		hoststatus = regs->status & statusmask;
	} while (!hoststatus);

	debug(mmc, "mmc command done\n");

	regs->clear = statusmask;
	if (hoststatus & SDI_STA_CTIMEOUT) {
		debug(mmc, "mmc cmd %i timed out\n", cmd->cmdidx);
		return -1;
	} else if ((hoststatus & SDI_STA_CCRCFAIL) &&
		   (cmd->resp_type & MMC_RSP_CRC)) {
		debug(mmc, "mmc cmd %i crc error\n", cmd->cmdidx);
		return -1;
	}

	debug(mmc, "mmc cmd %i good\n", cmd->cmdidx);

	if (cmd->resp_type & MMC_RSP_PRESENT) {
		cmd->response[0] = regs->response[0];
		cmd->response[1] = regs->response[1];
		cmd->response[2] = regs->response[2];
		cmd->response[3] = regs->response[3];
	}

	return 0;
}

static int 
do_command(struct mmc *mmc, struct mmc_cmd *cmd)
{
	volatile struct pl18x_regs *regs = mmc->base;
	uint32_t sdi_cmd = 0;
	int result;

	debug(mmc, "do_command %i\n", cmd->cmdidx);

	sdi_cmd = ((cmd->cmdidx & SDI_CMD_CMDINDEX_MASK) | SDI_CMD_CPSMEN);

	if (cmd->resp_type) {
		sdi_cmd |= SDI_CMD_WAITRESP;
		if (cmd->resp_type & MMC_RSP_136)
			sdi_cmd |= SDI_CMD_LONGRESP;
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

	return result;
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
	while ((!status_err) && (xfercount >= sizeof(u32))) {
		if (status & SDI_STA_RXDAVL) {
			*(tempbuff) = *regs->fifo;
			tempbuff++;
			xfercount -= sizeof(u32);
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
			if (xfercount >= SDI_FIFO_BURST_SIZE * sizeof(u32)) {
				for (i = 0; i < SDI_FIFO_BURST_SIZE; i++)
					*regs->fifo = *(tempbuff + i);
				tempbuff += SDI_FIFO_BURST_SIZE;
				xfercount -= SDI_FIFO_BURST_SIZE * sizeof(u32);
			} else {
				while (xfercount >= sizeof(u32)) {
					*regs->fifo = *(tempbuff);
					tempbuff++;
					xfercount -= sizeof(u32);
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
do_data_transfer(struct mmc *mmc, 
		struct mmc_cmd *cmd,
			    struct mmc_data *data)
{
	volatile struct pl18x_regs *regs = mmc->base;
	int error = ERR;

	uint32_t blksz = 0;
	uint32_t data_ctrl = 0;
	uint32_t data_len = (uint32_t) (data->blocks * data->blocksize);

	blksz = data->blocksize;
	data_ctrl |= (blksz << SDI_DCTRL_DBLOCKSIZE_V2_SHIFT);
	
	data_ctrl |= SDI_DCTRL_DTEN | SDI_DCTRL_BUSYMODE;

	regs->datatimer = SDI_DTIMER_DEFAULT;
	regs->datalength = data_len;
	udelay(DATA_REG_DELAY);

	if (data->flags & MMC_DATA_READ) {
		data_ctrl |= SDI_DCTRL_DTDIR_IN;
		regs->datactrl = data_ctrl;

		error = do_command(mmc, cmd);
		if (error)
			return error;

		error = read_bytes(mmc->base, data->dest, data->blocks,
				   data->blocksize);

	} else if (data->flags & MMC_DATA_WRITE) {
		error = do_command(mmc, cmd);
		if (error)
			return error;

		regs->datactrl = data_ctrl;
		error = write_bytes(mmc->base, data->src, data->blocks,
							data->blocksize);
	}

	return error;
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

	debug(mmc, "setting clock to 0x%h\n", sdi_clkcr);
	regs->clock = sdi_clkcr;
	udelay(CLK_CHANGE_DELAY);
	debug(mmc, "clock now 0x%h\n", regs->clock);

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
	u32 sdi_u32;

	regs->power = INIT_PWR;
	regs->clock = SDI_CLKCR_CLKDIV_INIT_V1 | SDI_CLKCR_CLKEN;

	udelay(CLK_CHANGE_DELAY);

	/* Disable mmc interrupts */
	sdi_u32 = regs->mask[0] & ~SDI_MASK0_MASK;
	regs->mask[0] = sdi_u32;
	
	return 0;
}

static volatile struct pl18x_regs * 
pl18x_map(void)
{
	volatile struct pl18x_regs *regs;
	size_t regs_pa, regs_len;
	struct proc0_req rq;
	struct proc0_rsp rp;

	regs_pa = 0x10000000 + (5 << 12);
	regs_len = 1 << 12;

	rq.type = PROC0_addr_req;
	rq.m.addr_req.pa = regs_pa;
	rq.m.addr_req.len = regs_len;

	send(0, (uint8_t *) &rq);
	while (recv(0, (uint8_t *) &rp) != 0)
		;

	if (rp.ret != OK) {
		return nil;
	}

	regs = (void *) PAGE_ALIGN(&_data_end);
	
	debug(nil, "mmc regs 0x%h mapping at 0x%h\n", regs_pa, regs);

	rq.type = PROC0_addr_map;
	rq.m.addr_map.pa = regs_pa;
	rq.m.addr_map.len = regs_len;
	rq.m.addr_map.va = (size_t) regs;
	rq.m.addr_map.flags = MAP_DEV|MAP_RW;

	send(0, (uint8_t *) &rq);
	while (recv(0, (uint8_t *) &rp) != 0)
		;

	if (rp.ret != OK) {
		return nil;
	}

	return regs;
}

void
main(void)
{
	volatile struct pl18x_regs *regs;
	struct mmc mmc;

	int ret;

	regs = pl18x_map();
	if (regs == nil) {
		debug(nil, "pl18x_map failed!\n");
		raise();
	}

	ret = pl18x_init(regs);
	if (ret != OK) {
		debug(nil, "pl18x_map failed!\n");
		raise();
	}

	mmc.base = regs;
	mmc.voltages = 0xff8080;

	mmc.command = &do_command;
	mmc.transfer = &do_data_transfer;
	mmc.set_ios = &pl18x_set_ios;
	mmc.reset = &pl18x_reset;
	
	mmc.debug = &debug;

	ret = mmc_start(&mmc);
	raise();
}

