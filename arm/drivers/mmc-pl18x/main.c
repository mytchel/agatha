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

#include "mmc.h"

int voltages = 0x00ff8080;
uint32_t csd[4];
	
size_t bl_len, csize, cmult, capacity;

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
mmc_send_if_cond(void)
{
	struct mmc_cmd cmd;
	int ret;

	cmd.cmdidx = SD_CMD_SEND_IF_COND;
	cmd.cmdarg = (voltages & 0xff8000) << 8 | 0xaa;
	cmd.resp_type = MMC_RSP_R7;

	ret = do_command(&cmd);
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
sd_send_op_cond(void)
{
	int timeout = 1000;
	int err;
	struct mmc_cmd cmd;

	while (1) {
		debug("sd_send_op_cond\n");

		cmd.cmdidx = MMC_CMD_APP_CMD;
		cmd.resp_type = MMC_RSP_R1;
		cmd.cmdarg = 0;

		err = do_command(&cmd);
		if (err != OK) {
			debug("error sending app cmd\n");
			return err;
		}

		cmd.cmdidx = SD_CMD_APP_SEND_OP_COND;
		cmd.resp_type = MMC_RSP_R3;
		cmd.cmdarg = voltages & 0xff8000;

		/* Other stuff for sd.2 and uhs? */
		
		err = do_command(&cmd);
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

	ocr = cmd.response[0];
	debug("ocr = 0x%h\n", ocr);
	if ((ocr & OCR_HCS) == OCR_HCS) {
		debug("high capacity\n");
	}
		
	rca = 0;

	return OK;
}

int
mmc_start_init(void)
{
	int ret;

	ret = mmc_go_idle();
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

	ret = mmc_send_if_cond();
	debug("if_cond ret %i\n", ret);

	ret = sd_send_op_cond();

	if (ret != OK) {
		/* Could be an MMC card. Should do something
			 else here. */
		return ret;
	}

	return OK;
}

int
mmc_startup(void)
{
	struct mmc_cmd cmd;
	uint32_t cid[4];
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
	ret = do_command(&cmd);

	if (ret != OK) {
		debug("send_cid failed\n");
		return ERR;
	}

	memcpy(cid, cmd.response, 16);

	debug("got cid 0x%h 0x%h 0x%h 0x%h\n", cid[0], cid[1], cid[2], cid[3]);

	cmd.cmdidx = SD_CMD_SEND_RELATIVE_ADDR;
	cmd.cmdarg = rca << 16;
	cmd.resp_type = MMC_RSP_R6;

	ret = do_command(&cmd);
	if (ret != OK) {
		debug("SD_CMD_SEND_RELATIVE_ADDR failed!\n");
		return ret;
	}

	/* If SD */
	rca = (cmd.response[0] >> 16) & 0xffff;

	cmd.cmdidx = MMC_CMD_SEND_CSD;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = rca << 16;

	ret = do_command(&cmd);
	if (ret != OK) {
		debug("MMC_CMD_SEND_CSD failed\n");
		return ret;
	}

	memcpy(csd, cmd.response, 16);

	bl_len = 1 << ((cmd.response[1] >> 16) & 0xf);

	debug("bl_len = 0x%h\n", bl_len);

	csize = (csd[1] & 0x3ff) << 2;
	cmult = (csd[2] & 0x00038000) >> 15;
	capacity = ((csize + 1) << (cmult + 2)) * bl_len;

	debug("csize = 0x%h, cmult = 0x%h, cap = 0x%h\n",
			csize, cmult, capacity);

	cmd.cmdidx = MMC_CMD_SELECT_CARD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = rca << 16;
	ret = do_command(&cmd);
	if (ret != OK) {
		debug("MMC_CMD_SELECT_CARD failed\n");
		return ret;
	}

	/* May have to do other stuff if this is not a SD card */

	/* Should do other stuff to set the bus width and speed */

	return OK;
}

uint8_t buffer[2048];

void
test(void)
{
	struct mmc_cmd cmd;
	struct mmc_data data;
	int ret;

	cmd.cmdidx = MMC_CMD_READ_SINGLE_BLOCK;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;

	data.dest = buffer;
	data.blocks = 1;
	data.blocksize = bl_len;
	data.flags = MMC_DATA_READ;

	ret = do_data_transfer(&cmd, &data);
	if (ret != OK) {
		debug("do_data_transfer failed\n");
		raise();
	}

	debug("data read:\n");
	int i;
	for (i = 0; i < bl_len; i++) {
		debug("0x%h : 0x%h\n", i, buffer[i]);
	}

}

	void
main(void)
{
	int ret;

	if (pl18x_map() != OK) {
		raise();
	}

	debug("mmc pl180 init\n");
	ret = pl18x_init();
	if (ret != OK)
		raise();

	debug("mmc start_init\n");
	ret = mmc_start_init();
	if (ret != OK)
		raise();

	debug("mmc startup\n");
	ret = mmc_startup();
	if (ret != OK)
		raise();

	test();

	debug("mmc loop\n");
	while (true) {

	}
}


