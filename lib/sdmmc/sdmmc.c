#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <log.h>
#include <stdarg.h>
#include <string.h>
#include <sdmmc.h>
#include <block.h>

int
mmc_go_idle(struct mmc *mmc)
{
	struct mmc_cmd cmd;

	cmd.cmdidx = MMC_CMD_GO_IDLE_STATE;
	cmd.cmdarg = 0;
	cmd.resp_type = MMC_RSP_NONE;

	log(LOG_INFO, "go idle");

	return mmc->command(mmc, &cmd, nil);
}

int
mmc_send_if_cond(struct mmc *mmc)
{
	struct mmc_cmd cmd;
	int ret;

	log(LOG_INFO, "send if cond");

	cmd.cmdidx = SD_CMD_SEND_IF_COND;
	cmd.cmdarg = (((mmc->voltages & 0xff8000) != 0) << 8) | 0xaa;
	cmd.resp_type = MMC_RSP_R7;

	ret = mmc->command(mmc, &cmd, nil);
	if (ret != OK) {
		return ret;
	}

	if ((cmd.response[0] & 0xff) != 0xaa) {
		log(LOG_WARNING, "bad response to if cond");
	} else {
		log(LOG_INFO, "This is a version 2 card");
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
		return err;
	}

	mmc->ocr = cmd.response[0];
	return OK;
}

static int
mmc_send_op_cond(struct mmc *mmc)
{
	int i, ret;;

	log(LOG_INFO, "mmc_send_op_cond");

	for (i = 0; i < 100; i++) {
		ret = mmc_send_op_cond_iter(mmc, i > 0);
		if (ret != OK) 
			return ret;

		if (mmc->ocr & OCR_BUSY) {
			break;
		}
	}

	if (!(mmc->ocr & OCR_BUSY)) {
		log(LOG_WARNING, "mmc_send_op_cond timeout");
		return ERR;
	}
	
	mmc->high_capacity = ((mmc->ocr & OCR_HCS) == OCR_HCS);
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
		log(LOG_INFO, "sd_send_op_cond");

		err = mmc->command(mmc, &app, nil);
		if (err != OK) {
			return err;
		}

		err = mmc->command(mmc, &cmd, nil);
		if (err != OK) {
			return err;
		}

		if (cmd.response[0] & OCR_BUSY) {
			break;
		} else if (timeout-- <= 0) {
			log(LOG_WARNING, "sd_send_op_cond timeout");
			return ERR;
		}
	}

	if (mmc->version != SD_VERSION_2)
		mmc->version = SD_VERSION_1_0;

	mmc->ocr = cmd.response[0];
	mmc->high_capacity = ((mmc->ocr & OCR_HCS) == OCR_HCS);
	mmc->rca = 0;

	return OK;
}

static int
mmc_send_all_cid(struct mmc *mmc)
{
	struct mmc_cmd cmd;
	int ret;

	log(LOG_INFO, "send cid");
	cmd.cmdidx = MMC_CMD_ALL_SEND_CID;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = 0;

	ret = mmc->command(mmc, &cmd, nil);
	if (ret != OK) {
		log(LOG_WARNING, "send_cid failed");
		return ERR;
	}

	memcpy(&mmc->cid, cmd.response, 16);

	return OK;
}

	static int
mmc_set_addr(struct mmc *mmc)
{
	struct mmc_cmd cmd;
	int ret;

	cmd.cmdidx = MMC_CMD_SET_RELATIVE_ADDR;
	cmd.cmdarg = mmc->rca << 16;
	cmd.resp_type = MMC_RSP_R6;

	ret = mmc->command(mmc, &cmd, nil);
	if (ret != OK) {
		log(LOG_WARNING, "mmc_set_addr failed!");
		return ret;
	}

	/* If SD */
	if (IS_SD(mmc)) {
		mmc->rca = cmd.response[0] >> 16 & 0xffff;
	}
	
	return OK;
}

	static int
mmc_send_csd(struct mmc *mmc)
{
	struct mmc_cmd cmd;
	uint32_t csize, cmult;
	int ret;

	cmd.cmdidx = MMC_CMD_SEND_CSD;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = mmc->rca << 16;

	ret = mmc->command(mmc, &cmd, nil);
	if (ret != OK) {
		log(LOG_WARNING, "mmc_send_csd failed");
		return ret;
	}

	mmc->csd[0] = cmd.response[0];
	mmc->csd[1] = cmd.response[1];
	mmc->csd[2] = cmd.response[2];
	mmc->csd[3] = cmd.response[3];

	log(LOG_INFO, "csd[0] = 0x%x", mmc->csd[0]);
	log(LOG_INFO, "csd[] = 0x%x", mmc->csd[1]);
	log(LOG_INFO, "csd[2] = 0x%x", mmc->csd[2]);
	log(LOG_INFO, "csd[3] = 0x%x", mmc->csd[3]);

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
	}

	if (mmc->high_capacity) {
		log(LOG_INFO, "high capacity");
		
		mmc->block_size = 512;

		csize = (mmc->csd[1] & 0x3f) << 16 
			| (mmc->csd[2] & 0xffff0000) >> 16;
		
		log(LOG_INFO, "csize = %i", csize);

		mmc->nblocks = ((uint64_t) (csize + 1)) << 10;
	} else {
		mmc->block_size = 1 << ((cmd.response[1] >> 16) & 0xf);

		csize = (mmc->csd[1] & 0x3ff) << 2 
			| (mmc->csd[2] & 0xc0000000) >> 30;
	
		cmult = (mmc->csd[2] & 0x00038000) >> 15;
	
		log(LOG_INFO, "csize = %i, cmult = %i", csize, cmult);
		mmc->nblocks = ((csize + 1) << (cmult + 2));
	}

	return OK;
}

	static int
mmc_select_card(struct mmc *mmc)
{
	struct mmc_cmd cmd;
	int ret;

	cmd.cmdidx = MMC_CMD_SELECT_CARD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = mmc->rca << 16;

	ret = mmc->command(mmc, &cmd, nil);
	if (ret != OK) {
		log(LOG_WARNING, "mmc_select_card failed");
		return ret;
	}

	return OK;
}

	static int
mmc_send_ext_csd(struct mmc *mmc, uint8_t *ext_csd)
{
	struct mmc_data data;
	struct mmc_cmd cmd;
	int err;

	log(LOG_INFO, "send ext csd");

	cmd.cmdidx = MMC_CMD_SEND_EXT_CSD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;

	data.dest = (void *) ext_csd;
	data.blocks = 1;
	data.blocksize = 512;
	data.flags = MMC_DATA_READ;

	err = mmc->command(mmc, &cmd, &data);
	if (err != OK) return ERR;

	uint32_t cap;
	cap = mmc->ext_csd[212]
		| mmc->ext_csd[213] << 8
		| mmc->ext_csd[214] << 16
		| mmc->ext_csd[215] << 24;

	log(LOG_INFO, "ext cap = %i", cap);
	if (cap > 0) {
		mmc->nblocks = cap;
	}

	return err;
}

	static int
mmc_startup(struct mmc *mmc)
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

	ret = sd_send_op_cond(mmc);
	if (ret != OK) {
		ret = mmc_send_op_cond(mmc);
		if (ret != OK) return ret;
	}

	ret = mmc_send_all_cid(mmc);
	if (ret != OK) return ret;

	ret = mmc_set_addr(mmc);
	if (ret != OK) return ret;

	ret = mmc_send_csd(mmc);
	if (ret != OK) return ret;

	ret = mmc_select_card(mmc);
	if (ret != OK) return ret;

	if (IS_SD(mmc)) {
		log(LOG_INFO, "sd ready");

	} else if (IS_MMC(mmc)) {
		ret = mmc_send_ext_csd(mmc, mmc->ext_csd);
		if (ret != OK) return ret;

		log(LOG_INFO, "mmc ready");
	}

	log(LOG_INFO, "block len = 0x%x", mmc->block_size);

	log(LOG_INFO, "nblocks = 0x%x * 1024", (uint32_t) (mmc->nblocks >> 10));

	mmc->capacity = mmc->nblocks * mmc->block_size;
	log(LOG_INFO, "capacity = %i Mb", (uint32_t) (mmc->capacity >> 20));

	return OK;
}

	int
mmc_read_block(struct mmc *mmc, size_t off, void *buffer)
{
	struct mmc_cmd cmd;
	struct mmc_data data;

	log(LOG_INFO, "reading 0x%x", off);

	cmd.cmdidx = MMC_CMD_READ_SINGLE_BLOCK;
	cmd.resp_type = MMC_RSP_R1;

	if (!mmc->high_capacity) {
		off *= mmc->block_size;
	}

	cmd.cmdarg = off;

	data.dest = buffer;
	data.blocks = 1;
	data.blocksize = mmc->block_size;
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

	if (!mmc->high_capacity) {
		off *= mmc->block_size;
	}

	cmd.cmdarg = off;

	data.dest = buffer;
	data.blocks = 1;
	data.blocksize = mmc->block_size;
	data.flags = MMC_DATA_WRITE;

	return mmc->command(mmc, &cmd, &data);
}

	int
read_blocks(struct block_dev *dev,
		void *buf, size_t start, size_t n)
{
	struct mmc *mmc = dev->arg;
	int i, ret;
	uint8_t *b;

	log(LOG_INFO, "read blocks 0x%x 0x%x", start, n);

	b = buf;

	for (i = 0; i < n; i++) {
		ret = mmc_read_block(mmc, start + i, b);
		if (ret != OK) {
			return ret;
		} else {
			b += mmc->block_size;
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
	uint8_t *b;

	log(LOG_INFO, "write blocks 0x%x 0x%x", start, n);

	b = buf;

	for (i = 0; i < n; i++) {
		ret = mmc_write_block(mmc, start + i, b);
		if (ret != OK) {
			return ret;
		} else {
			b += mmc->block_size;
		}
	}

	return OK;
}

	int
mmc_start(struct mmc *mmc)
{
	struct block_dev dev;
	int ret;

	log(LOG_INFO, "mmc startup");
	ret = mmc_startup(mmc);
	if (ret != OK) {
		log(LOG_WARNING, "mmc_startup failed");
		return ret;
	}

	dev.arg = mmc;
	dev.block_size = mmc->block_size;
	dev.nblocks = mmc->nblocks;
	dev.name = mmc->name;

	dev.map_buffers = true;
	dev.read_blocks_mapped = &read_blocks;
	dev.write_blocks_mapped = &write_blocks;

	log(LOG_INFO, "waiting for requests");

	ret = block_dev_register(&dev);
	if (ret != OK) {
		log(LOG_WARNING, "mmc block_dev_register failed");
		return ret;	
	}

	return OK;
}


