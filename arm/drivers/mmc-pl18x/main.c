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
static uint32_t rca;

void
debug(char *fmt, ...)
{
	char s[MESSAGE_LEN];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(s, sizeof(s), fmt, ap);
	va_end(ap);

	send(1, (uint8_t *) s);
	recv(1, (uint8_t *) s);
}

void
udelay(size_t us)
{
	size_t j;

	while (us-- > 0)
		for (j = 0; j < 1000; j++)
			;
}

static int 
wait_for_command_end(struct mmc_cmd *cmd)
{
	u32 hoststatus, statusmask;

	debug("mmc wait_for_command %i\n", cmd->cmdidx);

	statusmask = SDI_STA_CTIMEOUT | SDI_STA_CCRCFAIL;
	if ((cmd->resp_type & MMC_RSP_PRESENT))
		statusmask |= SDI_STA_CMDREND;
	else
		statusmask |= SDI_STA_CMDSENT;

	debug("mmc status mask = 0x%h\n", statusmask);
	do {
		debug("mmc status = 0x%h\n", regs->status);
		hoststatus = regs->status & statusmask;
		debug("mmc host status = 0x%h\n", hoststatus);
	} while (!hoststatus);

	debug("mmc command done\n");

	regs->status_clear = statusmask;
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

static int 
do_command(struct mmc_cmd *cmd)
{
	u32 sdi_cmd = 0;
	int result;

	sdi_cmd = ((cmd->cmdidx & SDI_CMD_CMDINDEX_MASK) | SDI_CMD_CPSMEN);

	if (cmd->resp_type) {
		sdi_cmd |= SDI_CMD_WAITRESP;
		if (cmd->resp_type & MMC_RSP_136)
			sdi_cmd |= SDI_CMD_LONGRESP;
	}

	regs->argument = cmd->cmdarg;
	udelay(COMMAND_REG_DELAY);
	debug("regs cmd = 0x%h\n", regs->command);
	debug("set  cmd = 0x%h\n", sdi_cmd);
	regs->command = sdi_cmd;
	debug("regs cmd = 0x%h\n", regs->command);
	result = wait_for_command_end(cmd);

	/* After CMD2 set RCA to a none zero value. */
	if ((result == 0) && (cmd->cmdidx == MMC_CMD_ALL_SEND_CID))
		rca = 10;

	/* After CMD3 open drain is switched off and push pull is used. */
	if ((result == 0) && (cmd->cmdidx == MMC_CMD_SET_RELATIVE_ADDR)) {
		u32 sdi_pwr = regs->power & ~SDI_PWR_OPD;
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

	regs->status_clear = SDI_ICR_MASK;

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

	regs->status_clear = SDI_ICR_MASK;

	if (xfercount) {
		return ERR;
	}

	return 0;
}

static int 
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
pl180_reset(void)
{
	regs->power = INIT_PWR;

	return 0;
}

int 
pl180_init(void)
{
	u32 sdi_u32;

	/* May need to enable a clock */

	debug("power now 0x%h\n", regs->power);
	debug("clock now 0x%h\n", regs->clock);

	debug("set power to 0x%h\n", INIT_PWR);
	regs->power = INIT_PWR;
	
	debug("set clocks to 0x%h\n", 
			SDI_CLKCR_CLKDIV_INIT_V1 | SDI_CLKCR_CLKEN);

	regs->clock = SDI_CLKCR_CLKDIV_INIT_V1 | SDI_CLKCR_CLKEN;

	udelay(CLK_CHANGE_DELAY);
/*
	clkin = ARM_MCLK;
	clk_min = ARM_MCLK / (2 * (SDI_CLKCR_CLKDIV_INIT_V1 + 1));
	clk_max = 6250000;
*/

	debug("clock now 0x%h\n", regs->clock);
	
	regs->clock |= SDI_CLKCR_CLKEN;
	
	udelay(CLK_CHANGE_DELAY);

	debug("power now 0x%h\n", regs->power);
	debug("clock now 0x%h\n", regs->clock);

	/* Disable mmc interrupts */
	sdi_u32 = regs->mask[0] & ~SDI_MASK0_MASK;
	regs->mask[0] = sdi_u32;
	
	return 0;
}

int
mmc_read_block(uint32_t blk, void *buf, size_t len)
{
	struct mmc_data data;
	struct mmc_cmd cmd;

	cmd.cmdidx = MMC_CMD_READ_SINGLE_BLOCK;
	cmd.cmdarg = blk;
	cmd.resp_type = MMC_RSP_R1;

	data.dest = buf;
	data.blocks = 1;
	data.blocksize = 512;
	data.flags = MMC_DATA_READ;

	return do_data_transfer(&cmd, &data);
}

int
mmc_go_idle(void)
{
	struct mmc_cmd cmd;

	debug("idle cmd %i\n", MMC_CMD_GO_IDLE_STATE);
	cmd.cmdidx = MMC_CMD_GO_IDLE_STATE;
	cmd.cmdarg = 0;
	cmd.resp_type = MMC_RSP_NONE;

	debug("go idle\n");

	return do_command(&cmd);
}

int
sd_send_op_cond(void)
{
	return OK;
}

int
mmc_init(void)
{
	int ret;

	/*
	ret = mmc_set_bus_width(1);
	if (ret != OK)
		return ret;

	ret = mmc_set_clock(MMC_CLK_ENABLE);
	if (ret != OK)
		return ret;

		set_ios below will set the bus width to 1
		and clock to full speed?
	
	host_set_ios();
*/

	debug("go idle\n");
	ret = mmc_go_idle();
	if (ret != OK)
		return ret;

	debug("idle\n");
	/*
	ret = mmc_send_if_cond();
	if (ret != OK)
		return ret;
*/

	ret = sd_send_op_cond();
	if (ret != OK)
		return ret;

	return OK;
}

int
mmc_startup(void)
{
	struct mmc_cmd cmd;
	int ret;

	cmd.cmdidx = MMC_CMD_SEND_CID;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = 0;

	ret = do_command(&cmd);
	if (ret != OK)
		return ret;

	return OK;
}

	void
main(void)
{
	size_t regs_pa, regs_len;
	struct proc0_req rq;
	struct proc0_rsp rp;
	int ret;

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
		raise();
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

	if (rp.ret != OK) {
		raise();
	}

	debug("mmc regs mapped\n");

	debug("mmc pl180 init\n");
	ret = pl180_init();
	if (ret != OK)
		raise();

	debug("mmc init\n");
	ret = mmc_init();
	if (ret != OK)
		raise();

	debug("mmc startup\n");
	ret = mmc_startup();
	if (ret != OK)
		raise();

	debug("mmc loop\n");
	while (true) {

	}
}


