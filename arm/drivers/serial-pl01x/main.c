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
handle_write(int eid, int from, union serial_req *rq)
{
	union serial_rsp rp;
	size_t i;

	rp.write.type = SERIAL_write_rsp;
	rp.write.ret = OK;

	for (i = 0; i < rq->write.len; i++)
		putc(rq->write.data[i]);

	reply(eid, from, &rp);
}

	void
handle_read(int eid, int from, union serial_req *rq)
{
	union serial_rsp rp;

	rp.read.type = SERIAL_read_rsp;
	rp.read.ret = ERR;

	reply(eid, from, &rp);
}

	void
main(void)
{
	uint8_t m[MESSAGE_LEN];
	int from;

	while (true) {
		if (recv(EID_SUPERVISOR, &from, m) < 0) {
			exit(1);
		}

		exit(0);
	}

#if 0
	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];
	char dev_name[MESSAGE_LEN];

	size_t regs_pa, regs_len;
	union dev_reg_req drq;
	union dev_reg_rsp drp;
	union serial_req rq;
	int eid, from;

	recv(0, init_m);
	regs_pa = init_m[0];
	regs_len = init_m[1];
	
	recv(0, dev_name);

	regs = map_addr(regs_pa, regs_len, MAP_DEV|MAP_RW);
	if (regs == nil) {
		exit(ERR);
	}

	drq.type = DEV_REG_register_req;
	drq.reg.pid = pid();
	snprintf(drq.reg.name, sizeof(drq.reg.name),
			"%s", dev_name);

	if (mesg(DEV_REG_PID, &drq, &drp) != OK) {
		exit(ERR);
	}

	if (drp.reg.ret != OK) {
		exit(drp.reg.ret);
	}

	puts("pl01x ready at ");
	puts(dev_name);
	puts("\n");
	
	while (true) {
		if ((eid = recv(EID_ANY, &from, &rq)) < 0) continue;
		if (from == PID_NONE) continue;

		switch (rq.type) {
			case SERIAL_write_req:
				handle_write(eid, from, &rq);
				break;

			case SERIAL_read_req:
				handle_read(eid, from, &rq);
				break;
		}
	}
#endif
}

