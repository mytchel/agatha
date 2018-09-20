/* Heavily modified from U-Boot with help from the
	 SD Host Controller Simplified Specification Version 4.20. 

 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Ulf Hansson <ulf.hansson@stericsson.com>
 * Author: Martin Lundholm <martin.xa.lundholm@stericsson.com>
 * Ported to drivers/mmc/ by: Matt Waddel <matt.waddel@linaro.org>
 */


#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <m.h>
#include <stdarg.h>
#include <string.h>

#include "mmc.h"

void
debug(char *fmt, ...);

int
mmc_go_idle(struct mmc *mmc)
{
	struct mmc_cmd cmd;

	debug("idle cmd %i\n", MMC_CMD_GO_IDLE_STATE);
	cmd.cmdidx = MMC_CMD_GO_IDLE_STATE;
	cmd.cmdarg = 0;
	cmd.resp_type = MMC_RSP_NONE;

	debug("go idle\n");

	return mmc->command(mmc, &cmd);
}

int
mmc_send_if_cond(struct mmc *mmc)
{
	struct mmc_cmd cmd;
	int ret;

	cmd.cmdidx = SD_CMD_SEND_IF_COND;
	cmd.cmdarg = (mmc->voltages & 0xff8000) << 8 | 0xaa;
	cmd.resp_type = MMC_RSP_R7;

	ret = mmc->command(mmc, &cmd);
	if (ret != OK) {
		return ret;
	}

	debug("got response 0x%h\n", cmd.response[0]);

	if ((cmd.response[0] & 0xff) != 0xaa) {
		debug("bad response to if cond\n");
	} else {
		debug("This is a version 2 card\n");
	}

	return OK;
}

int
sd_send_op_cond(struct mmc *mmc)
{
	int timeout = 1000;
	int err;
	struct mmc_cmd cmd;

	while (1) {
		debug("sd_send_op_cond\n");

		cmd.cmdidx = MMC_CMD_APP_CMD;
		cmd.resp_type = MMC_RSP_R1;
		cmd.cmdarg = 0;

		err = mmc->command(mmc, &cmd);
		if (err != OK) {
			debug("error sending app cmd\n");
			return err;
		}

		cmd.cmdidx = SD_CMD_APP_SEND_OP_COND;
		cmd.resp_type = MMC_RSP_R3;
		cmd.cmdarg = mmc->voltages & 0xff8000;

		/* Other stuff for sd.2 and uhs? */
		
		err = mmc->command(mmc, &cmd);
		if (err != OK) {
			debug("error sending sd send op cond\n");
			return err;
		}

		if (cmd.response[0] & OCR_BUSY) {
			debug("got response?\n");
			break;
		}

		if (timeout-- <= 0) {
			debug("timeout\n");
			return ERR;
		}
	}

	debug("so far so good\n");

	mmc->ocr = cmd.response[0];
	debug("ocr = 0x%h\n", mmc->ocr);
	if ((mmc->ocr & OCR_HCS) == OCR_HCS) {
		debug("high capacity\n");
	}
		
	mmc->rca = 0;

	return OK;
}

int
mmc_start_init(struct mmc *mmc)
{
	int ret;

	ret = mmc_go_idle(mmc);
	if (ret != OK) 
		return ret;

	/*
	ret = mmc_set_bus_width(1);
	if (ret != OK)
		return ret;

	ret = mmc_set_clock(MMC_CLK_ENABLE);
	if (ret != OK)
		return ret;
*/
	/*
		set_ios below will set the bus width to 1
		and clock to full speed?
*/
/*	
	host_set_ios();
*/

	ret = mmc_send_if_cond(mmc);
	debug("if_cond ret %i\n", ret);

	ret = sd_send_op_cond(mmc);

	if (ret != OK) {
		/* Could be an MMC card. Should do something
			 else here. */
		return ret;
	}

	return OK;
}

int
mmc_startup(struct mmc *mmc)
{
	struct mmc_cmd cmd;
	size_t csize, cmult;
	int ret;

	/* Why is this failing? Is it needed?
	 From SD Host Controller Simplified Spcificaiton
	 I'm inclined to say it is not needed or wanted.
	 Or, it's failure means this is an SDIO card. Which 
	 this is not. */

	cmd.cmdidx = MMC_CMD_ALL_SEND_CID;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = 0;

	debug("try send cid\n");
	ret = mmc->command(mmc, &cmd);

	if (ret != OK) {
		debug("send_cid failed\n");
		return ERR;
	}

	memcpy(&mmc->cid, cmd.response, 16);

	cmd.cmdidx = SD_CMD_SEND_RELATIVE_ADDR;
	cmd.cmdarg = mmc->rca << 16;
	cmd.resp_type = MMC_RSP_R6;

	ret = mmc->command(mmc, &cmd);
	if (ret != OK) {
		debug("SD_CMD_SEND_RELATIVE_ADDR failed!\n");
		return ret;
	}

	/* If SD */
	mmc->rca = (cmd.response[0] >> 16) & 0xffff;

	cmd.cmdidx = MMC_CMD_SEND_CSD;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = mmc->rca << 16;

	ret = mmc->command(mmc, &cmd);
	if (ret != OK) {
		debug("MMC_CMD_SEND_CSD failed\n");
		return ret;
	}

	memcpy(mmc->csd, cmd.response, 16);

	mmc->block_len = 1 << ((cmd.response[1] >> 16) & 0xf);

	debug("bl_len = 0x%h\n", mmc->block_len);

	csize = (mmc->csd[1] & 0x3ff) << 2;
	cmult = (mmc->csd[2] & 0x00038000) >> 15;
	mmc->capacity = ((csize + 1) << (cmult + 2)) * mmc->block_len;

	debug("csize = 0x%h, cmult = 0x%h, cap = 0x%h\n",
			csize, cmult, mmc->capacity);

	cmd.cmdidx = MMC_CMD_SELECT_CARD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = mmc->rca << 16;
	ret = mmc->command(mmc, &cmd);
	if (ret != OK) {
		debug("MMC_CMD_SELECT_CARD failed\n");
		return ret;
	}

	/* May have to do other stuff if this is not a SD card */

	/* Should do other stuff to set the bus width and speed */

	return OK;
}

uint8_t buffer[2048];

int
mmc_read_block(struct mmc *mmc, size_t blk, void *buffer)
{
	struct mmc_cmd cmd;
	struct mmc_data data;

	cmd.cmdidx = MMC_CMD_READ_SINGLE_BLOCK;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = blk;

	data.dest = buffer;
	data.blocks = 1;
	data.blocksize = mmc->block_len;
	data.flags = MMC_DATA_READ;

	return mmc->transfer(mmc, &cmd, &data);
}

	int
mmc_write_block(struct mmc *mmc, size_t blk, void *buffer)
{
	struct mmc_cmd cmd;
	struct mmc_data data;

	cmd.cmdidx = MMC_CMD_WRITE_SINGLE_BLOCK;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = blk;

	data.dest = buffer;
	data.blocks = 1;
	data.blocksize = mmc->block_len;
	data.flags = MMC_DATA_WRITE;

	return mmc->transfer(mmc, &cmd, &data);
}

void
test(struct mmc *mmc)
{
	int ret, i;

	debug("do data blk 0\n");
	ret = mmc_read_block(mmc, 0, buffer);
	if (ret != OK) {
		debug("mmc_read-block failed\n");
		raise();
	}

	debug("data read:\n");
	for (i = 0; i < mmc->block_len; i++) {
		debug("0x%h : 0x%h\n", i, buffer[i]);
	}

	uint8_t t[32] = "Hello there.";
	memcpy(buffer + 0x1e0, t, sizeof(t));
	
	ret = mmc_write_block(mmc, 0, buffer);
	if (ret != OK) {
		debug("mmc_write_block failed\n");
		raise();
	}
}

int
mmc_start(struct mmc *mmc)
{
	int ret;

	debug("mmc start_init\n");
	ret = mmc_start_init(mmc);
	if (ret != OK)
		raise();

	debug("mmc startup\n");
	ret = mmc_startup(mmc);
	if (ret != OK)
		raise();

	test(mmc);

	debug("mmc loop\n");
	while (true) {

	}
}


