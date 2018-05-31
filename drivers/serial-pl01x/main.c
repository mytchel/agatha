#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <m.h>
#include <arm/pl01x.h>

static struct pl01x_regs *regs;

extern uint32_t *_data_end;

  static void
putc(char c)
{
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
get_serial(void)
{
}

void
main(void)
{
	size_t regs_pa, regs_len;
	struct proc0_req rq;
	struct proc0_rsp rp;

	regs_pa = 0x10000000 + (9 << 12);
	regs_len = 1 << 12;

	rq.from = pid();
	rq.type = PROC0_addr_req;
	rq.m.addr_req.pa = regs_pa;
	rq.m.addr_req.len = regs_len;

	send(0, (uint8_t *) &rq);
	while (recv((uint8_t *) &rp) != OK || rp.from != 0)
		;

	if (rp.ret != OK) {
		raise();
	}

	rq.from = pid();
	rq.type = PROC0_addr_map;
	rq.m.addr_map.pa = regs_pa;
	rq.m.addr_map.len = regs_len;
	rq.m.addr_map.va = PAGE_ALIGN(&_data_end);
	rq.m.addr_map.flags = MAP_DEV|MAP_RW;

	send(0, (uint8_t *) &rq);
	while (recv((uint8_t *) &rp) != OK || rp.from != 0)
		;

	if (rp.ret != OK) {
		raise();
	}

	regs = (void *) rq.m.addr_map.va;
	
	puts("user pl01x ready\n");
	
	while (true) {
		recv((uint8_t *) &rq);
		puts((char *) rq.m.raw);
		rp.from = pid();
		send(rq.from, (uint8_t *) &rp);
	}
}

