#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <mesg.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <dev_reg.h>
#include <arm/omap3_uart.h>
#include <serial.h>

static volatile struct omap3_uart_regs *regs;

void
log(void)
{

}

	static void
putc(char c)
{
	if (c == '\n')
		putc('\r');

	while ((regs->lsr & (1 << 5)) == 0)
		;

	regs->hr = c; 
}

	void
puts(const char *c)
{
	while (*c)
		putc(*c++);
}

	void
handle_write(int from, union serial_req *rq)
{
	union serial_rsp rp;
	size_t i;

	rp.write.type = SERIAL_write;
	rp.write.ret = OK;

	for (i = 0; i < rq->write.len; i++)
		putc(rq->write.data[i]);

	send(from, &rp);
}

	void
handle_read(int from, union serial_req *rq)
{
	union serial_rsp rp;

	rp.read.type = SERIAL_read;
	rp.read.ret = ERR;

	send(from, &rp);
}

	void
main(void)
{
	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];
	char name[MESSAGE_LEN];

	size_t regs_pa, regs_len;
	union dev_reg_req drq;
	union dev_reg_rsp drp;
	union serial_req rq;
	int from;

	recv(0, init_m);

	regs_pa = init_m[0];
	regs_len = init_m[1];

	recv(0, name);

	regs = map_addr(regs_pa, regs_len, MAP_DEV|MAP_RW);
	if (regs == nil) {
		exit();
	}

	drq.type = DEV_REG_register;
	drq.reg.pid = pid();
	snprintf(drq.reg.name, sizeof(drq.reg.name),
			"%s", name);

	if (mesg(DEV_REG_PID, &drq, &drp) != OK) {
		exit();
	}

	if (drp.reg.ret != OK) {
		exit();
	}

	char buf[64];
	snprintf(buf, sizeof(buf),
			"serial_omap3 ready at pid %i on %s\n", pid(), name);
	puts(buf);

	while (true) {
		from = recv(-1, &rq);
		if (from < 0) continue;

		switch (rq.type) {
			case SERIAL_write:
				handle_write(from, &rq);
				break;

			case SERIAL_read:
				handle_read(from, &rq);
				break;
		}
	}
}

