#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <m.h>
#include <arm/omap3_uart.h>

static volatile struct omap3_uart_regs *regs;

extern uint32_t *_data_end;

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
main(void)
{
	size_t regs_pa, regs_len;
	struct proc0_req rq;
	struct proc0_rsp rp;
	uint8_t m[MESSAGE_LEN];
	int from;

	regs_pa = 0x44e09000;
	regs_len = 0x2000;

	rq.type = PROC0_addr_req;
	rq.m.addr_req.pa = regs_pa;
	rq.m.addr_req.len = regs_len;

	send(0, (uint8_t *) &rq);
	while (recv(0, (uint8_t *) &rp) != 0)
		;

	if (rp.ret != OK) {
		raise();
	}

	rq.type = PROC0_addr_map;
	rq.m.addr_map.pa = regs_pa;
	rq.m.addr_map.len = regs_len;
	rq.m.addr_map.va = PAGE_ALIGN(&_data_end);
	rq.m.addr_map.flags = MAP_DEV|MAP_RW;

	send(0, (uint8_t *) &rq);
	while (recv(0, (uint8_t *) &rp) != 0)
		;

	if (rp.ret != OK) {
		raise();
	}

	regs = (void *) rq.m.addr_map.va;
	
	puts("user uart-omap3 ready\n");
	
	while (true) {
		from = recv(-1, m);
		puts((char *) m);
		send(from, m);
	}
}

