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
#include <proc0.h>
#include <stdarg.h>
#include <string.h>
#include <sdmmc.h>
#include <block_dev.h>

int
mmc_go_idle(struct mmc *mmc)
{
	struct mmc_cmd cmd;

	mmc->debug(mmc, "idle cmd %i\n", MMC_CMD_GO_IDLE_STATE);
	cmd.cmdidx = MMC_CMD_GO_IDLE_STATE;
	cmd.cmdarg = 0;
	cmd.resp_type = MMC_RSP_NONE;

	mmc->debug(mmc, "go idle\n");

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

	mmc->debug(mmc, "got response 0x%h\n", cmd.response[0]);

	if ((cmd.response[0] & 0xff) != 0xaa) {
		mmc->debug(mmc, "bad response to if cond\n");
	} else {
		mmc->debug(mmc, "This is a version 2 card\n");
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
		mmc->debug(mmc, "sd_send_op_cond\n");

		cmd.cmdidx = MMC_CMD_APP_CMD;
		cmd.resp_type = MMC_RSP_R1;
		cmd.cmdarg = 0;

		err = mmc->command(mmc, &cmd);
		if (err != OK) {
			mmc->debug(mmc, "error sending app cmd\n");
			return err;
		}

		cmd.cmdidx = SD_CMD_APP_SEND_OP_COND;
		cmd.resp_type = MMC_RSP_R3;
		cmd.cmdarg = mmc->voltages & 0xff8000;

		/* Other stuff for sd.2 and uhs? */
		
		err = mmc->command(mmc, &cmd);
		if (err != OK) {
			mmc->debug(mmc, "error sending sd send op cond\n");
			return err;
		}

		if (cmd.response[0] & OCR_BUSY) {
			break;
		}

		if (timeout-- <= 0) {
			mmc->debug(mmc, "timeout\n");
			return ERR;
		}
	}

	mmc->ocr = cmd.response[0];
	mmc->debug(mmc, "ocr = 0x%h\n", mmc->ocr);
	if ((mmc->ocr & OCR_HCS) == OCR_HCS) {
		mmc->debug(mmc, "high capacity\n");
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
	mmc->set_ios(mmc);
*/

	ret = mmc_send_if_cond(mmc);
	mmc->debug(mmc, "if_cond ret %i\n", ret);

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

	ret = mmc->command(mmc, &cmd);
	if (ret != OK) {
		mmc->debug(mmc, "send_cid failed\n");
		return ERR;
	}

	memcpy(&mmc->cid, cmd.response, 16);

	cmd.cmdidx = SD_CMD_SEND_RELATIVE_ADDR;
	cmd.cmdarg = mmc->rca << 16;
	cmd.resp_type = MMC_RSP_R6;

	ret = mmc->command(mmc, &cmd);
	if (ret != OK) {
		mmc->debug(mmc, "SD_CMD_SEND_RELATIVE_ADDR failed!\n");
		return ret;
	}

	/* If SD */
	mmc->rca = (cmd.response[0] >> 16) & 0xffff;

	cmd.cmdidx = MMC_CMD_SEND_CSD;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = mmc->rca << 16;

	ret = mmc->command(mmc, &cmd);
	if (ret != OK) {
		mmc->debug(mmc, "MMC_CMD_SEND_CSD failed\n");
		return ret;
	}

	memcpy(mmc->csd, cmd.response, 16);

	csize = (mmc->csd[1] & 0x3ff) << 2;
	cmult = (mmc->csd[2] & 0x00038000) >> 15;
	
	mmc->block_len = 1 << ((cmd.response[1] >> 16) & 0xf);
	mmc->nblocks = ((csize + 1) << (cmult + 2));
	mmc->capacity = mmc->nblocks * mmc->block_len;

	mmc->debug(mmc, "block len = %i, nblocks = %i, capacity = %iMb\n",
			mmc->block_len, mmc->nblocks, mmc->capacity >> 20);

	cmd.cmdidx = MMC_CMD_SELECT_CARD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = mmc->rca << 16;
	ret = mmc->command(mmc, &cmd);
	if (ret != OK) {
		mmc->debug(mmc, "MMC_CMD_SELECT_CARD failed\n");
		return ret;
	}

	/* May have to do other stuff if this is not a SD card */

	/* Should do other stuff to set the bus width and speed */

	return OK;
}

int
mmc_read_block(struct mmc *mmc, size_t off, void *buffer)
{
	struct mmc_cmd cmd;
	struct mmc_data data;

	cmd.cmdidx = MMC_CMD_READ_SINGLE_BLOCK;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = off;

	data.dest = buffer;
	data.blocks = 1;
	data.blocksize = mmc->block_len;
	data.flags = MMC_DATA_READ;

	return mmc->transfer(mmc, &cmd, &data);
}

	int
mmc_write_block(struct mmc *mmc, size_t off, void *buffer)
{
	struct mmc_cmd cmd;
	struct mmc_data data;

	cmd.cmdidx = MMC_CMD_WRITE_SINGLE_BLOCK;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = off;

	data.dest = buffer;
	data.blocks = 1;
	data.blocksize = mmc->block_len;
	data.flags = MMC_DATA_WRITE;

	return mmc->transfer(mmc, &cmd, &data);
}

int
read_blocks(struct block_dev *dev,
		void *buf, size_t start, size_t n)
{
	struct mmc *mmc = dev->arg;
	int i, ret;

	for (i = 0; i < n; i += mmc->block_len) {
		ret = mmc_read_block(mmc, start + i, 
				(uint8_t *) buf + i);
		if (ret != OK) {
			return ret;
		}
	}

	return OK;
}

int
write_blocks(struct block_dev *dev,
		void *buf, size_t start, size_t n)
{
	struct mmc *mmc = dev->arg;
	int i, ret;

	for (i = 0; i < n; i += mmc->block_len) {
		ret = mmc_write_block(mmc, start + i, 
				(uint8_t *) buf + i);
		if (ret != OK) {
			return ret;
		}
	}

	return OK;
}

int
mmc_start(struct mmc *mmc)
{
	struct block_dev dev;
	int ret;

	mmc->debug(mmc, "mmc start_init\n");
	ret = mmc_start_init(mmc);
	if (ret != OK) {
		mmc->debug(mmc, "mmc_start_init failed\n");
		return ret;
	}

	mmc->debug(mmc, "mmc startup\n");
	ret = mmc_startup(mmc);
	if (ret != OK) {
		mmc->debug(mmc, "mmc_startup failed\n");
		return ret;
	}

	dev.arg = mmc;
	dev.block_len = mmc->block_len;
	dev.nblocks = mmc->nblocks;
	dev.name = mmc->name;

	dev.read_blocks = &read_blocks;
	dev.write_blocks = &write_blocks;

	ret = block_dev_register(&dev);
	if (ret != OK) {
		mmc->debug(mmc, "mmc block_dev_register failed\n");
		return ret;	
	}

	return OK;
}


