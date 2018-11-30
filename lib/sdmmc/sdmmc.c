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

	return mmc->command(mmc, &cmd, nil);
}

int
mmc_send_if_cond(struct mmc *mmc)
{
	struct mmc_cmd cmd;
	int ret;

	mmc->debug(mmc, "send if cond\n");

	cmd.cmdidx = SD_CMD_SEND_IF_COND;
	cmd.cmdarg = (((mmc->voltages & 0xff8000) != 0) << 8) | 0xaa;
	cmd.resp_type = MMC_RSP_R7;

	ret = mmc->command(mmc, &cmd, nil);
	if (ret != OK) {
		return ret;
	}

	mmc->debug(mmc, "got response 0x%x\n", cmd.response[0]);

	if ((cmd.response[0] & 0xff) != 0xaa) {
		mmc->debug(mmc, "bad response to if cond\n");
	} else {
		mmc->debug(mmc, "This is a version 2 card\n");
		mmc->version = SD_VERSION_2;
	}

	return OK;
}

static int
mmc_send_op_cond_iter(struct mmc *mmc, bool use_arg)
{
	struct mmc_cmd cmd;
	int err;

	cmd.cmdidx = MMC_CMD_SEND_OP_COND;
	cmd.resp_type = MMC_RSP_R3;
	if (use_arg) {
		cmd.cmdarg = OCR_HCS | 
			(mmc->voltages & (mmc->ocr & OCR_VOLTAGE_MASK)) |
			(mmc->ocr & OCR_ACCESS_MODE);
	} else {
		cmd.cmdarg = 0;
	}

	err = mmc->command(mmc, &cmd, nil);
	if (err != OK) {
		mmc->debug(mmc, "error sending mmc send op cond\n");
		return err;
	}

	mmc->ocr = cmd.response[0];
	return OK;
}

static void
udelay(size_t us)
{
	while (us > 0)
		us--;
}

static int
mmc_send_op_cond(struct mmc *mmc)
{
	int i, ret;;

	mmc->debug(mmc, "mmc_send_op_cond\n");

	for (i = 0; i < 100; i++) {
		ret = mmc_send_op_cond_iter(mmc, i > 0);
		if (ret != OK) 
			return ret;

		if (mmc->ocr & OCR_BUSY) {
			mmc->debug(mmc, "not busy\n");
			break;
		}
	
		mmc->debug(mmc, "got ocr of 0x%x\n", mmc->ocr);
		mmc->debug(mmc, "mmc_send_op_cond again\n");
		udelay(100);
	}

	if (!(mmc->ocr & OCR_BUSY)) {
		mmc->debug(mmc, "timeout\n");
		return ERR;
	}
	
	mmc->debug(mmc, "ocr = 0x%x\n", mmc->ocr);

	if ((mmc->ocr & OCR_HCS) == OCR_HCS) {
		mmc->debug(mmc, "high capacity\n");
	}

	mmc->version = MMC_VERSION_UNKNOWN;
	mmc->rca = 1;

	return OK;
}

	static int
sd_send_op_cond(struct mmc *mmc)
{
	struct mmc_cmd cmd, app;
	int timeout = 10;
	int err;

	app.cmdidx = MMC_CMD_APP_CMD;
	app.resp_type = MMC_RSP_R1;
	app.cmdarg = 0;

	cmd.cmdidx = SD_CMD_APP_SEND_OP_COND;
	cmd.resp_type = MMC_RSP_PRESENT;
	cmd.cmdarg = (mmc->voltages & 0xff8000);

	if (mmc->version == SD_VERSION_2)
		cmd.cmdarg |= OCR_HCS;

	while (1) {
		mmc->debug(mmc, "sd_send_op_cond\n");

		err = mmc->command(mmc, &app, nil);
		if (err != OK) {
			mmc->debug(mmc, "error sending app cmd\n");
			return err;
		}

		err = mmc->command(mmc, &cmd, nil);
		if (err != OK) {
			mmc->debug(mmc, "error sending mmc send op cond\n");
			return err;
		}

		if (!(cmd.response[0] & OCR_BUSY)) {
			mmc->debug(mmc, "busy\n");
			break;
		} else if (timeout-- <= 0) {
			mmc->debug(mmc, "timeout\n");
			return ERR;
		}
	}

	if (mmc->version != SD_VERSION_2)
		mmc->version = SD_VERSION_1_0;

	mmc->ocr = cmd.response[0];
	mmc->debug(mmc, "ocr = 0x%x\n", mmc->ocr);
	if ((mmc->ocr & OCR_HCS) == OCR_HCS) {
		mmc->debug(mmc, "high capacity\n");
	}

	mmc->rca = 0;

	return OK;
}

	static int
mmc_send_ext_csd(struct mmc *mmc, uint8_t *ext_csd)
{
	struct mmc_cmd cmd;
	struct mmc_data data;
	int err;

	mmc->debug(mmc, "send ext csd\n");

	cmd.cmdidx = MMC_CMD_SEND_EXT_CSD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;

	data.dest = (void *) ext_csd;
	data.blocks = 1;
	data.blocksize = 512;
	data.flags = MMC_DATA_READ;

	err = mmc->command(mmc, &cmd, &data);

	return err;
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

	/*
	uint8_t csd[512];
	ret = mmc_send_ext_csd(mmc, csd);
	if (ret != OK) {
		mmc->debug(mmc, "send ext csd failed %i\n", ret);
	}
*/

	ret = mmc_send_if_cond(mmc);
	mmc->debug(mmc, "if_cond ret %i\n", ret);

	ret = sd_send_op_cond(mmc);
	mmc->debug(mmc, "op cond ret %i\n", ret);
	if (ret == OK) {
		mmc->debug(mmc, "sd card 2.0 or later\n");

	} else {
		mmc->debug(mmc, "mmc or sd card 1.0\n");

		ret = mmc_send_op_cond(mmc);
	}

	return ret;
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

	mmc->debug(mmc, "send cid\n");
	cmd.cmdidx = MMC_CMD_ALL_SEND_CID;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = 0;

	ret = mmc->command(mmc, &cmd, nil);
	if (ret != OK) {
		mmc->debug(mmc, "send_cid failed\n");
		return ERR;
	}

	memcpy(&mmc->cid, cmd.response, 16);

	cmd.cmdidx = MMC_CMD_SET_RELATIVE_ADDR;
	cmd.cmdarg = mmc->rca << 16;
	cmd.resp_type = MMC_RSP_R6;

	ret = mmc->command(mmc, &cmd, nil);
	if (ret != OK) {
		mmc->debug(mmc, "SD_CMD_SEND_RELATIVE_ADDR failed!\n");
		return ret;
	}

	/* If SD */
	if (IS_SD(mmc)) {
		mmc->rca = cmd.response[0] >> 16 & 0xffff;
	}

	cmd.cmdidx = MMC_CMD_SEND_CSD;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = mmc->rca << 16;

	ret = mmc->command(mmc, &cmd, nil);
	if (ret != OK) {
		mmc->debug(mmc, "MMC_CMD_SEND_CSD failed\n");
		return ret;
	}

	memcpy(mmc->csd, cmd.response, 16);

	if (mmc->version == MMC_VERSION_UNKNOWN) {
		int version = (cmd.response[0] >> 26) & 0xf;

		switch (version) {
			case 0:
				mmc->version = MMC_VERSION_1_2;
				break;
			case 1:
				mmc->version = MMC_VERSION_1_4;
				break;
			case 2:
				mmc->version = MMC_VERSION_2_2;
				break;
			case 3:
				mmc->version = MMC_VERSION_3;
				break;
			case 4:
				mmc->version = MMC_VERSION_4;
				break;
			default :
				mmc->version = MMC_VERSION_1_2;
		}

		mmc->debug(mmc, "version = %i / %i\n", version, mmc->version);
	}

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

	ret = mmc->command(mmc, &cmd, nil);
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

	return mmc->command(mmc, &cmd, &data);
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

	return mmc->command(mmc, &cmd, &data);
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


