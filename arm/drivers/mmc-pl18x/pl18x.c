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
#include "mmc.h"

extern uint32_t *_data_end;

static volatile struct pl18x_regs *regs;

uint32_t rca, ocr;

static int 
wait_for_command_end(struct mmc_cmd *cmd)
{
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

	debug("mmc command done\n");

	regs->clear = statusmask;
	if (hoststatus & SDI_STA_CTIMEOUT) {
		debug("mmc cmd %i timed out\n", cmd->cmdidx);
		return -1;
	} else if ((hoststatus & SDI_STA_CCRCFAIL) &&
		   (cmd->resp_type & MMC_RSP_CRC)) {
		debug("mmc cmd %i crc error\n", cmd->cmdidx);
		return -1;
	}

	debug("mmc cmd %i good\n", cmd->cmdidx);

	if (cmd->resp_type & MMC_RSP_PRESENT) {
		cmd->response[0] = regs->response[0];
		cmd->response[1] = regs->response[1];
		cmd->response[2] = regs->response[2];
		cmd->response[3] = regs->response[3];
	}

	return 0;
}

int 
do_command(struct mmc_cmd *cmd)
{
	uint32_t sdi_cmd = 0;
	int result;

	debug("do_command %i\n", cmd->cmdidx);

	sdi_cmd = ((cmd->cmdidx & SDI_CMD_CMDINDEX_MASK) | SDI_CMD_CPSMEN);

	if (cmd->resp_type) {
		sdi_cmd |= SDI_CMD_WAITRESP;
		if (cmd->resp_type & MMC_RSP_136)
			sdi_cmd |= SDI_CMD_LONGRESP;
	}

	regs->argument = cmd->cmdarg;
	udelay(COMMAND_REG_DELAY);
	regs->command = sdi_cmd;
	result = wait_for_command_end(cmd);

	/* After CMD2 set RCA to a none zero value. */
	if ((result == 0) && (cmd->cmdidx == MMC_CMD_ALL_SEND_CID))
		rca = 10;

	/* After CMD3 open drain is switched off and push pull is used. */
	if ((result == 0) && (cmd->cmdidx == MMC_CMD_SET_RELATIVE_ADDR)) {
		uint32_t sdi_pwr = regs->power & ~SDI_PWR_OPD;
		regs->power = sdi_pwr;
	}

	return result;
}

static int 
read_bytes(u32 *dest, u32 blkcount, u32 blksize)
{
	u64 xfercount = blkcount * blksize;
	u32 *tempbuff = dest;
	u32 status, status_err;

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
write_bytes(u32 *src, u32 blkcount, u32 blksize)
{
	u32 *tempbuff = src;
	u64 xfercount = blkcount * blksize;
	u32 status, status_err;
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

int 
do_data_transfer(struct mmc_cmd *cmd,
			    struct mmc_data *data)
{
	int error = ERR;
	u32 blksz = 0;
	u32 data_ctrl = 0;
	u32 data_len = (u32) (data->blocks * data->blocksize);

	blksz = data->blocksize;
	data_ctrl |= (blksz << SDI_DCTRL_DBLOCKSIZE_V2_SHIFT);
	
	data_ctrl |= SDI_DCTRL_DTEN | SDI_DCTRL_BUSYMODE;

	regs->datatimer = SDI_DTIMER_DEFAULT;
	regs->datalength = data_len;
	udelay(DATA_REG_DELAY);

	if (data->flags & MMC_DATA_READ) {
		data_ctrl |= SDI_DCTRL_DTDIR_IN;
		regs->datactrl = data_ctrl;

		error = do_command(cmd);
		if (error)
			return error;

		error = read_bytes((u32 *)data->dest, (u32)data->blocks,
				   (u32)data->blocksize);
	} else if (data->flags & MMC_DATA_WRITE) {
		error = do_command(cmd);
		if (error)
			return error;

		regs->datactrl = data_ctrl;
		error = write_bytes((u32 *)data->src, (u32)data->blocks,
							(u32)data->blocksize);
	}

	return error;
}

int
host_set_ios(void)
{
	u32 sdi_clkcr;

	sdi_clkcr = regs->clock;

	sdi_clkcr &= ~(SDI_CLKCR_CLKDIV_MASK);
	sdi_clkcr |= 0;

	sdi_clkcr &= ~(SDI_CLKCR_WIDBUS_MASK);
	sdi_clkcr |= SDI_CLKCR_WIDBUS_1;

	debug("setting clock to 0x%h\n", sdi_clkcr);
	regs->clock = sdi_clkcr;
	udelay(CLK_CHANGE_DELAY);
	debug("clock now 0x%h\n", regs->clock);

	return 0;
}

int
pl18x_reset(void)
{
	regs->power = INIT_PWR;

	return 0;
}

int 
pl18x_init(void)
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

int
pl18x_map(void)
{
	size_t regs_pa, regs_len;
	struct proc0_req rq;
	struct proc0_rsp rp;

	debug("mmc debug test\n");

	regs_pa = 0x10000000 + (5 << 12);
	regs_len = 1 << 12;

	rq.type = PROC0_addr_req;
	rq.m.addr_req.pa = regs_pa;
	rq.m.addr_req.len = regs_len;

	send(0, (uint8_t *) &rq);
	while (recv(0, (uint8_t *) &rp) != 0)
		;

	if (rp.ret != OK) {
		return rp.ret;
	}

	regs = (void *) PAGE_ALIGN(&_data_end);
	
	debug("mmc regs 0x%h mapping at 0x%h\n", regs_pa, regs);

	rq.type = PROC0_addr_map;
	rq.m.addr_map.pa = regs_pa;
	rq.m.addr_map.len = regs_len;
	rq.m.addr_map.va = (size_t) regs;
	rq.m.addr_map.flags = MAP_DEV|MAP_RW;

	send(0, (uint8_t *) &rq);
	while (recv(0, (uint8_t *) &rp) != 0)
		;

	return rp.ret;
}


