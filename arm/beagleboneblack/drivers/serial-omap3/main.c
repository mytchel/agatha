#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <dev_reg.h>
#include <arm/omap3_uart.h>

static volatile struct omap3_uart_regs *regs;

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
	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];
	char name[MESSAGE_LEN];

	size_t regs_pa, regs_len;
	union dev_reg_req drq;
	union dev_reg_rsp drp;
	uint8_t m[MESSAGE_LEN];
	int from;

	recv(0, init_m);

	regs_pa = init_m[0];
	regs_len = init_m[1];
	
	recv(0, name);

	regs = map_addr(regs_pa, regs_len, MAP_DEV|MAP_RW);
	if (regs == nil) {
		exit();
	}

	puts("serial_omap3 test");

	drq.type = DEV_REG_register;
	drq.reg.pid = pid();
	snprintf(drq.reg.name, sizeof(drq.reg.name),
			"%s", name);

	send(DEV_REG_PID, (uint8_t *) &drq);
	while (recv(DEV_REG_PID, (uint8_t *) &drp) != DEV_REG_PID)
		;

	if (drp.reg.ret != OK) {
		exit();
	}

	puts("serial_omap3 ready at ");
	puts(name);
	puts("\n");
	
	while (true) {
		from = recv(-1, m);
		puts((char *) m);
		send(from, m);
	}
}

