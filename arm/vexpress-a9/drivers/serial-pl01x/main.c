#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <dev_reg.h>
#include <arm/pl01x.h>

static volatile struct pl01x_regs *regs;

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
main(void)
{
	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];

	char *name = "serial0";
	size_t regs_pa, regs_len;
	union dev_reg_req drq;
	union dev_reg_rsp drp;
	uint8_t m[MESSAGE_LEN];
	int from;

	recv(-1, init_m);
	regs_pa = init_m[0];
	regs_len = init_m[1];

	regs = request_device(regs_pa, regs_len, MAP_DEV|MAP_RW);
	if (regs == nil) {
		raise();
	}

	drq.type = DEV_REG_register;
	drq.reg.pid = pid();
	snprintf(drq.reg.name, sizeof(drq.reg.name),
			"%s", name);

	send(DEV_REG_PID, (uint8_t *) &drq);
	while (recv(DEV_REG_PID, (uint8_t *) &drp) != DEV_REG_PID)
		;

	if (drp.reg.ret != OK) {
		raise();
	}

	puts("pl01x ready at ");
	puts(name);
	puts("\n");
	
	while (true) {
		from = recv(-1, m);
		puts((char *) m);
		send(from, m);
	}
}

