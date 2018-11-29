#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <dev_reg.h>
#include <arm/aml_meson8_uart.h>

static volatile struct aml_meson8_uart_regs *regs;

  static void
putc(char c)
{
	if (c == '\n')
		putc('\r');

	while ((regs->status & UART_STATUS_TX_FULL))
		;

	regs->wfifo = c;  
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

	size_t regs_pa, regs_len, off;
	union dev_reg_req drq;
	union dev_reg_rsp drp;
	uint8_t m[MESSAGE_LEN];
	int from;

	recv(0, init_m);

	regs_pa = init_m[0];
	regs_len = init_m[1];
	
	recv(0, name);

	off = regs_pa - PAGE_ALIGN_DN(regs_pa);
	regs_pa = PAGE_ALIGN_DN(regs_pa);
	regs_len = PAGE_ALIGN(regs_len);
	
	regs = map_addr(regs_pa, regs_len, MAP_DEV|MAP_RW);
	if (regs == nil) {
		exit();
	}

	regs = (void *) ((size_t) regs + off);

	puts("aml_serial test");

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

	puts("aml_serial ready at ");
	puts(name);
	puts("\n");
	
	while (true) {
		from = recv(-1, m);
		puts((char *) m);
		send(from, m);
	}
}

