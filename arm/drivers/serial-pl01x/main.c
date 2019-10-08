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

	rp.write.type = SERIAL_write;
	rp.write.ret = OK;

	for (i = 0; i < rq->write.len; i++)
		putc(rq->write.data[i]);

	reply(eid, from, &rp);
}

	void
handle_read(int eid, int from, union serial_req *rq)
{
	union serial_rsp rp;

	rp.read.type = SERIAL_read;
	rp.read.ret = ERR;

	reply(eid, from, &rp);
}

	void
main(int p_eid)
{
	size_t regs_pa, regs_len;
	union serial_req rq;
	union proc0_req prq;
	union proc0_rsp prp;
	int eid, from;

	parent_eid = p_eid;

	prq.get_resource.type = PROC0_get_resource;
	prq.get_resource.resource_type = RESOURCE_type_regs;

	mesg(parent_eid, &prq, &prp);

	if (prp.get_resource.ret != OK) {
		exit(ERR);
	}

	regs_pa  = prp.get_resource.result.regs.pa;
	regs_len = prp.get_resource.result.regs.len;

	regs = map_addr(regs_pa, regs_len, MAP_DEV|MAP_RW);
	if (regs == nil) {
		exit(ERR);
	}

	puts("serial pl01x ready\n");
	
	while (true) {
		if ((eid = recv(EID_ANY, &from, &rq)) < 0) continue;
		if (from == PID_NONE) continue;

		switch (rq.type) {
			case SERIAL_write:
				handle_write(eid, from, &rq);
				break;

			case SERIAL_read:
				handle_read(eid, from, &rq);
				break;
		}
	}
}

