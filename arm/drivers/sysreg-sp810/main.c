#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <m.h>
#include <stdarg.h>
#include <string.h>
#include <arm/sp810.h>

static volatile struct sp810_regs *regs;

extern uint32_t *_data_end;

void
debug(char *fmt, ...)
{
	char s[MESSAGE_LEN];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(s, sizeof(s), fmt, ap);
	va_end(ap);

	send(1, (uint8_t *) s);
	recv(1, (uint8_t *) s);
}

void
main(void)
{
	size_t regs_pa, regs_len;
	struct proc0_req rq;
	struct proc0_rsp rp;

	regs_pa = 0x10000000 + (0 << 12);
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

	debug("sysreg mapped from 0x%h to 0x%h\n", regs_pa, regs);	
	debug("id 0x%h\n", regs->id);	
	debug("mci address 0x%h\n", &regs->mci);	
	debug("mci 0x%h\n", regs->mci);	

	while (true)
		;
}

