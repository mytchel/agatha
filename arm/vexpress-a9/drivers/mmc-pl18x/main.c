#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <m.h>
#include <arm/pl18x.h>

static struct pl18x_regs *regs;

extern uint32_t *_data_end;

void
main(void)
{
	size_t regs_pa, regs_len;
	uint8_t m[MESSAGE_LEN];
	struct proc0_req rq;
	struct proc0_rsp rp;

	regs_pa = 0x10000000 + (5 << 12);
	regs_len = 1 << 12;

	rq.type = PROC0_addr_req;
	rq.m.addr_req.pa = regs_pa;
	rq.m.addr_req.len = regs_len;

	send(0, (uint8_t *) &rq);
	while (recv(0, (uint8_t *) &rp) != 0)
		;

	if (rp.ret != OK) {
		raise();
	}

	regs = (void *) PAGE_ALIGN(&_data_end);

	rq.type = PROC0_addr_map;
	rq.m.addr_map.pa = regs_pa;
	rq.m.addr_map.len = regs_len;
	rq.m.addr_map.va = (size_t) regs;
	rq.m.addr_map.flags = MAP_DEV|MAP_RW;

	send(0, (uint8_t *) &rq);
	while (recv(0, (uint8_t *) &rp) != 0)
		;

	if (rp.ret != OK) {
		raise();
	}
	
	while (true) {
		recv(-1, m);
	}
}

