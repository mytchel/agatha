#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <m.h>
#include <arm/pl01x.h>

static struct pl01x_regs *regs;

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
	rq.type = PROC0_mem_req;
	rq.m.mem_req.pa = regs_pa;
	rq.m.mem_req.va = 0;
	rq.m.mem_req.len = regs_len;
	rq.m.mem_req.flags = MAP_DEV|MAP_RW;

	send(0, (uint8_t *) &rq);
	while (recv((uint8_t *) &rp) != OK || rp.from != 0)
		;

	regs = (void *) rp.m.mem_req.addr;
	
	puts("user pl01x ready\n");
	
	while (true) {
		recv((uint8_t *) &rq);
		puts((char *) rq.m.raw);
		rp.from = pid();
		send(rq.from, (uint8_t *) &rp);
	}
}

