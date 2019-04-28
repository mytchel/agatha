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
#include <arm/pl01x.h>
#include <serial.h>

static volatile struct pl01x_regs *regs;

  static void
putc(char c)
{
	if (c == '\n')
		putc('\r');

	while ((regs->fr & UART_PL01x_FR_TXFF))
		;

	regs->dr = c;  
}

  void
puts(const char *c)
{
  while (*c)
    putc(*c++);
}

void
log(void)
{

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
	char dev_name[MESSAGE_LEN];

	size_t regs_pa, regs_len;
	union dev_reg_req drq;
	union dev_reg_rsp drp;
	union serial_req rq;
	int from;

	recv(0, init_m);
	regs_pa = init_m[0];
	regs_len = init_m[1];
	
	recv(0, dev_name);

	regs = map_addr(regs_pa, regs_len, MAP_DEV|MAP_RW);
	if (regs == nil) {
		exit();
	}

	drq.type = DEV_REG_register;
	drq.reg.pid = pid();
	snprintf(drq.reg.name, sizeof(drq.reg.name),
			"%s", dev_name);

	if (mesg(DEV_REG_PID, (uint8_t *) &drq, &drp) != OK) {
		exit();
	}

	if (drp.reg.ret != OK) {
		exit();
	}

	puts("pl01x ready at ");
	puts(dev_name);
	puts("\n");
	
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

