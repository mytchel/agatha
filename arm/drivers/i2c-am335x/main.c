#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <dev_reg.h>
#include <arm/am335x_i2c.h>
#include <i2c.h>
#include <log.h>

static char dev_name[MESSAGE_LEN];
static volatile struct am335x_i2c_regs *regs;
static bool configured = false;

int
get_device_pid(char *name)
{
	union dev_reg_req rq;
	union dev_reg_rsp rp;

	rq.find.type = DEV_REG_find_req;
	rq.find.block = true;
	snprintf(rq.find.name, sizeof(rq.find.name),
			"%s", name);

	if (mesg(DEV_REG_PID, &rq, &rp) != OK) {
		return ERR;
	} else if (rp.find.ret != OK) {
		return ERR;
	} else {
		return rp.find.pid;
	}
}

static void
udelay(size_t us)
{
	while (us-- > 0) {
		int j;
		for (j = 0; j < 10; j++)
			;
	}
}

int
i2c_init(int oa, size_t speed_kHz)
{
	regs->con = 0;
	udelay(50000);

	regs->sysc = 2;

	udelay(1000);
	regs->con = I2C_CON_EN;
	while (!regs->syss) {
		log(LOG_INFO, "syss   = 0x%x", regs->syss);
		udelay(1000);
	}

	/* disable */
	regs->con = 0;

	/* TODO: configurable speed */

	/* prescaller to get 12 MHz module clock 
	configure for 400 kHz from clock 
	 tLow  = (SCLL + 7) * ICLK time period 
	 tHigh = (SCLH + 5) * ICLK */

	regs->psc = 7;
	regs->scll = 13;
	regs->sclh = 5;

	regs->con = I2C_CON_EN;
	regs->irqenable_set = I2C_IRQ_BB |
		I2C_IRQ_XRDY |
		I2C_IRQ_RRDY |
		I2C_IRQ_ARDY |
		I2C_IRQ_NACK;
	regs->irqenable_clr = 0xffffffff;
	
	regs->oa = oa;

	configured = true;

	return OK;
}

	int
i2c_read(int slave, int sub, uint8_t *buf, size_t len)
{
	size_t alen = 1;
	uint32_t stat;

	log(LOG_INFO, "read 0x%x . 0x%x %i", slave, sub, len);
	regs->irqstatus = 0xffff;
	udelay(1000);
	while ((regs->irqstatus_raw & I2C_IRQ_BB)) {
		log(LOG_INFO, "wr bb status = 0x%x", regs->irqstatus_raw);
		regs->irqstatus = regs->irqstatus_raw;
	}
		
	regs->irqstatus = 0xffff;

	regs->sa = slave;
	regs->cnt = alen;
	regs->con = I2C_CON_EN |
		I2C_CON_MST |
		I2C_CON_TRX |
		I2C_CON_STT;

	while (1) {
		stat = regs->irqstatus_raw;

		if (stat & I2C_IRQ_NACK) {
			log(LOG_WARNING, "ra error 0x%x", stat);
			return ERR;
		} else if (alen > 0 && (stat & I2C_IRQ_XRDY)) {
			regs->data = sub;
			regs->irqstatus = I2C_IRQ_XRDY;
			alen--;
		} else if (stat & I2C_IRQ_ARDY) {
			regs->irqstatus = I2C_IRQ_ARDY;
			break;
		}
	}

	regs->sa = slave;
	regs->cnt = len;
	regs->con = I2C_CON_EN |
		I2C_CON_MST |
		I2C_CON_STP |
		I2C_CON_STT;

	while (len > 0) {
		stat = regs->irqstatus_raw;

		if (stat & I2C_IRQ_NACK) {
			log(LOG_WARNING, "rd error 0x%x", stat);
			return ERR;
		} else if (stat & I2C_IRQ_ARDY) {
			regs->irqstatus = I2C_IRQ_ARDY;
			break;
		} else if (stat & I2C_IRQ_RRDY) {
			*buf++ = regs->data;
			regs->irqstatus  = I2C_IRQ_RRDY;
		}
	}

	return OK;
}

	int
i2c_write(int slave, int sub, uint8_t *buf, size_t len)
{
	size_t alen = 1;
	uint32_t stat;

	log(LOG_INFO, "write 0x%x . 0x%x %i", slave, sub, len);
	regs->irqstatus = 0xffff;
	udelay(1000);
	while ((regs->irqstatus_raw & I2C_IRQ_BB)) {
		log(LOG_INFO, "wr bb status = 0x%x", regs->irqstatus_raw);
		regs->irqstatus = regs->irqstatus_raw;
	}
		
	regs->irqstatus = 0xffff;

	regs->sa = slave;
	regs->cnt = alen + len;
	regs->con = I2C_CON_EN |
		I2C_CON_MST |
		I2C_CON_TRX |
		I2C_CON_STP |
		I2C_CON_STT;

	while (alen > 0) {
		stat = regs->irqstatus_raw;

		if (stat & I2C_IRQ_XRDY) {
			regs->data = sub;
			regs->irqstatus = I2C_IRQ_XRDY;
			alen--;
		} else if (stat & I2C_IRQ_NACK) {
			log(LOG_WARNING, "wa error 0x%x", stat);
			return ERR;
		}
	}

	while (len > 0) {
		stat = regs->irqstatus_raw;

		if (stat & I2C_IRQ_XRDY) {
			regs->data = *buf++;
			regs->irqstatus  = I2C_IRQ_XRDY;
			len--;
		} else {
			log(LOG_WARNING, "wd error 0x%x", stat);
			return ERR;
		}
	}

	return OK;
}

static int
handle_configure(int from, 
		union i2c_req *rq)
{
	union i2c_rsp rp;

	rp.configure.type = I2C_configure_rsp;
	rp.configure.ret = i2c_init(rq->configure.addr,
			rq->configure.speed_kHz);

	return send(from, &rp);
}

static int
handle_read(int from, 
		union i2c_req *rq) 
{
	union i2c_rsp rp;

	rp.read.type = I2C_read_rsp;
	if (!configured) {
		rp.read.ret = ERR;
		return send(from, &rp);
	}

	rp.read.ret = i2c_read(rq->read.slave, rq->read.addr,
			rp.read.buf, rq->read.len);

	return send(from, &rp);
}


static int
handle_write(int from, 
		union i2c_req *rq)
{
	union i2c_rsp rp;

	rp.write.type = I2C_write_rsp;
	if (!configured) {
		rp.write.ret = ERR;
		return send(from, &rp);
	}

	rp.write.ret = i2c_write(rq->write.slave, rq->write.addr,
		 rq->write.buf, rq->write.len);

	return send(from, &rp);
}

	void
main(void)
{
	int from;

	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];

	size_t regs_pa, regs_len;
	union dev_reg_req drq;
	union dev_reg_rsp drp;

	recv(0, init_m);

	regs_pa = init_m[0];
	regs_len = init_m[1];

	recv(0, dev_name);

	log_init(dev_name);

	regs = map_addr(regs_pa, regs_len, MAP_DEV|MAP_RW);
	if (regs == nil) {
		log(LOG_FATAL, "failed to map registers!");
		exit(ERR);
	}

	log(LOG_INFO, "on pid %i mapped 0x%x -> 0x%x", pid(), regs_pa, regs);

	/* wait for prm to set up everything */
	int prm_cm_pid;
	do {
		prm_cm_pid = get_device_pid("prm-cm0");
	} while (prm_cm_pid < 0);

	uint8_t m[MESSAGE_LEN];
	mesg(prm_cm_pid, m, m);
	
#if 1
	if (strcmp(dev_name, "i2c0")) {
		uint8_t buf[1];

		i2c_init(0x40, 400);

		/* i2c0 0x70 is a tda19988 */
#if 0
		/* This doesn't work for some reason */

		uint8_t addr = 0x70;
		i2c_read(addr, 0xff, buf, sizeof(buf));
		log(LOG_INFO, "read 0x%x", buf[0]);
		buf[0] = 0x0;
		i2c_write(addr, 0xff, buf, sizeof(buf));
		i2c_read(addr, 0xff, buf, sizeof(buf));
		log(LOG_INFO, "read 0x%x", buf[0]);

		i2c_read(addr, 0x00, buf, sizeof(buf));
		log(LOG_INFO, "read 0x%x", buf[0]);
		i2c_read(addr, 0x02, buf, sizeof(buf));
		log(LOG_INFO, "read 0x%x", buf[0]);
	#endif

		/* i2c0 0x24 is a tps65217 */
		i2c_read(0x24, 0, buf, sizeof(buf));
		log(LOG_INFO, "read 0x%x", buf[0]);
		i2c_read(0x24, 0x07, buf, sizeof(buf));
		log(LOG_INFO, "read 0x%x", buf[0]);
		i2c_read(0x24, 0x08, buf, sizeof(buf));
		log(LOG_INFO, "read 0x%x", buf[0]);

		buf[0] = (1<<3);
		i2c_write(0x24, 0x07, buf, sizeof(buf));
		i2c_read(0x24, 0x07, buf, sizeof(buf));
		log(LOG_INFO, "read 0x%x", buf[0]);

		/* Flashes the power led by enabling and
			 disabling LDO2 */
		bool on = false;
		int i;
		for (i = 0; i < 10; i++) {
			/* 0x13 is password protected level 2 
				 so the dance must be danced */
			log(LOG_INFO, "flash led %i", on);

			buf[0] = 0x7d ^ 0x13;
			i2c_write(0x24, 0x0b, buf, sizeof(buf));
			buf[0] = on ? 0x3f : 0;
			i2c_write(0x24, 0x13, buf, sizeof(buf));
			
			buf[0] = 0x7d ^ 0x13;
			i2c_write(0x24, 0x0b, buf, sizeof(buf));
			buf[0] = on ? 0x3f : 0;
			i2c_write(0x24, 0x13, buf, sizeof(buf));

			i2c_read(0x24, 0x13, buf, sizeof(buf));
			log(LOG_INFO, "read 0x%x", buf[0]);
					
			udelay(50000);
			on = !on;
		}
	}
#endif

	drq.type = DEV_REG_register_req;
	drq.reg.pid = pid();
	snprintf(drq.reg.name, sizeof(drq.reg.name),
			"%s", dev_name);

	if (mesg(DEV_REG_PID, &drq, &drp) != OK || drp.reg.ret != OK) {
		log(LOG_WARNING, "failed to register with dev reg!");
		exit(ERR);
	}

	while (true) {
		union i2c_req rq;

		if ((from = recv(-1, &rq)) < 0)
			continue;

		switch (rq.type) {
			case I2C_configure_req:
				handle_configure(from, &rq);
				break;

			case I2C_read_req:
				handle_read(from, &rq);
				break;

			case I2C_write_req:
				handle_write(from, &rq);
				break;
		}
	}
}

