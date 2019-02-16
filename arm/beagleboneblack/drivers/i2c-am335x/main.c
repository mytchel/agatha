#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <dev_reg.h>
#include <arm/am335x_i2c.h>

static volatile struct am335x_i2c_regs *regs;
static char dev_name[MESSAGE_LEN];

int
get_device_pid(char *name)
{
	union dev_reg_req rq;
	union dev_reg_rsp rp;

	rq.find.type = DEV_REG_find;
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

void
debug(char *fmt, ...)
{
	static int pid = -1;
	while (pid < 0) {
		pid = get_device_pid("serial0");
	}

	char s[MESSAGE_LEN];
	va_list ap;

	snprintf(s, sizeof(s), "%s: ", dev_name);
	va_start(ap, fmt);
	vsnprintf(s + strlen(s),
			sizeof(s) - strlen(s),
			fmt, ap);
	va_end(ap);

	mesg(pid, (uint8_t *) s, (uint8_t *) s);
}

static void
udelay(size_t us)
{
	size_t j;
	while (us-- > 0)
		for (j = 0; j < 100; j++)
			;
}

int
i2c_init(int oa)
{
	regs->con = 0;
	udelay(50000);

	regs->sysc = 2;

	udelay(1000);
	regs->con = I2C_CON_EN;
	while (!regs->syss) {
		debug("syss   = 0x%x\n", regs->syss);
		udelay(1000);
	}

	/* disable */
	regs->con = 0;

	/* prescaller to get 12 MHz module clock 
	configure for 400 kHz from clock 
	 tLow  = (SCLL + 7) * ICLK time period 
	 tHigh = (SCLH + 5) * ICLK */

#if 0	
	regs->psc = 7;
	regs->scll = 13;
	regs->sclh = 5;
#else
	regs->psc = 7;
	regs->scll = 13;
	regs->sclh = 5;
#endif

	regs->con = I2C_CON_EN;
	regs->irqenable_set = I2C_IRQ_BB |
		I2C_IRQ_XRDY |
		I2C_IRQ_RRDY |
		I2C_IRQ_ARDY |
		I2C_IRQ_NACK;
	regs->irqenable_clr = 0xffffffff;
	
	regs->oa = oa;

	return OK;
}

	int
i2c_read(int slave, int sub, uint8_t *buf, size_t len)
{
	size_t alen = 1;
	uint32_t stat;

	debug("read 0x%x . 0x%x %i\n", slave, sub, len);
	regs->irqstatus = 0xffff;
	udelay(1000);
	while ((regs->irqstatus_raw & I2C_IRQ_BB)) {
		debug("wr bb status = 0x%x\n", regs->irqstatus_raw);
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
		debug("wa status = 0x%x\n", stat);

		if (stat & I2C_IRQ_NACK) {
			debug("wa error 0x%x\n", stat);
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
		debug("rd status = 0x%x\n", stat);

		if (stat & I2C_IRQ_NACK) {
			debug("rd error 0x%x\n", stat);
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

	debug("write 0x%x . 0x%x %i\n", slave, sub, len);
	regs->irqstatus = 0xffff;
	udelay(1000);
	while ((regs->irqstatus_raw & I2C_IRQ_BB)) {
		debug("wr bb status = 0x%x\n", regs->irqstatus_raw);
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
		debug("wa status = 0x%x\n", stat);

		if (stat & I2C_IRQ_XRDY) {
			regs->data = sub;
			regs->irqstatus = I2C_IRQ_XRDY;
			alen--;
		} else if (stat & I2C_IRQ_NACK) {
			debug("wa error 0x%x\n", stat);
			return ERR;
		}
	}

	while (len > 0) {
		stat = regs->irqstatus_raw;
		debug("wd status = 0x%x\n", stat);

		if (stat & I2C_IRQ_XRDY) {
			regs->data = *buf++;
			regs->irqstatus  = I2C_IRQ_XRDY;
			len--;
		} else {
			debug("wd error 0x%x\n", stat);
			return ERR;
		}
	}

	return OK;
}

	void
main(void)
{
	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];

	size_t regs_pa, regs_len;
	union dev_reg_req drq;
	union dev_reg_rsp drp;

	recv(0, init_m);

	regs_pa = init_m[0];
	regs_len = init_m[1];

	recv(0, dev_name);

	debug("test\n");

	regs = map_addr(regs_pa, regs_len, MAP_DEV|MAP_RW);
	if (regs == nil) {
		debug("failed to map registers!\n");
		exit();
	}

	debug("mapped 0x%x -> 0x%x\n", regs_pa, regs);

	/* wait for prm to set up everything */
	int prm_cm_pid;
	do {
		prm_cm_pid = get_device_pid("prm-cm0");
	} while (prm_cm_pid < 0);

	uint8_t m[MESSAGE_LEN];
	mesg(prm_cm_pid, m, m);

	debug("try read something\n");

	i2c_init(0x40);
	uint8_t buf[1];

	if (strcmp(dev_name, "i2c0")) {
		/* i2c0 0x24 is a tps65217 */
		i2c_read(0x24, 0, buf, sizeof(buf));
		debug("read 0x%x\n", buf[0]);
		i2c_read(0x24, 0x07, buf, sizeof(buf));
		debug("read 0x%x\n", buf[0]);
		i2c_read(0x24, 0x08, buf, sizeof(buf));
		debug("read 0x%x\n", buf[0]);

		buf[0] = (1<<3);
		i2c_write(0x24, 0x07, buf, sizeof(buf));
		i2c_read(0x24, 0x07, buf, sizeof(buf));
		debug("read 0x%x\n", buf[0]);

		/* Flashes the power led by enabling and
			 disabling LDO2 */
		bool on = false;
		while (true) {
			/* 0x13 is password protected level 2 
				 so the dance must be danced */
			buf[0] = 0x7d ^ 0x13;
			i2c_write(0x24, 0x0b, buf, sizeof(buf));
			buf[0] = on ? 0x3f : 0;
			i2c_write(0x24, 0x13, buf, sizeof(buf));
			buf[0] = 0x7d ^ 0x13;
			i2c_write(0x24, 0x0b, buf, sizeof(buf));
			buf[0] = on ? 0x3f : 0;
			i2c_write(0x24, 0x13, buf, sizeof(buf));

			i2c_read(0x24, 0x13, buf, sizeof(buf));
			debug("read 0x%x\n", buf[0]);
					
			udelay(10000);
			on = !on;
		}

	} else if (strcmp(dev_name, "i2c2")) {
		/*
		i2c_read(112, 0, buf, sizeof(buf));
		debug("read 0x%x\n", buf[0]);
		i2c_read(52, 0, buf, sizeof(buf));
		i2c_read(52, 0, buf, sizeof(buf));
		debug("read 0x%x\n", buf[0]);
		*/
	}

	drq.type = DEV_REG_register;
	drq.reg.pid = pid();
	snprintf(drq.reg.name, sizeof(drq.reg.name),
			"%s", dev_name);

	send(DEV_REG_PID, (uint8_t *) &drq);
	while (recv(DEV_REG_PID, (uint8_t *) &drp) != DEV_REG_PID)
		;

	if (drp.reg.ret != OK) {
		debug("failed to register with dev reg!\n");
		exit();
	}

	debug("mapped and registered\n");

	while (true) {
		recv(-1, init_m);
	}
}

