#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <proc0.h>
#include <arm/pl01x.h>

static volatile struct pl01x_regs *regs;

extern uint32_t *_data_end;

  static void
putc(char c)
{
	while ((regs->fr & UART_PL01x_FR_TXFF))
		;

	regs->dr = c;  

	if (c == '\n')
		putc('\r');
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
	union proc0_req rq;
	union proc0_rsp rp;
	uint8_t m[MESSAGE_LEN];
	int from;

	regs_pa = 0x10000000 + (9 << 12);
	regs_len = 1 << 12;

	rq.addr_req.type = PROC0_addr_req;
	rq.addr_req.pa = regs_pa;
	rq.addr_req.len = regs_len;

	send(0, (uint8_t *) &rq);
	while (recv(0, (uint8_t *) &rp) != 0)
		;

	if (rp.addr_req.ret != OK) {
		raise();
	}

	rq.addr_map.type = PROC0_addr_map;
	rq.addr_map.pa = regs_pa;
	rq.addr_map.len = regs_len;
	rq.addr_map.va = PAGE_ALIGN(&_data_end);
	rq.addr_map.flags = MAP_DEV|MAP_RW;

	send(0, (uint8_t *) &rq);
	while (recv(0, (uint8_t *) &rp) != 0)
		;

	if (rp.addr_map.ret != OK) {
		raise();
	}

	regs = (void *) rq.addr_map.va;
	
	puts("user pl01x ready\n");
	
	while (true) {
		from = recv(-1, m);
		puts((char *) m);
		send(from, m);
	}
}

