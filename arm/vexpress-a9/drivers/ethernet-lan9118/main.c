#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <dev_reg.h>
#include <arm/lan9118.h>

static volatile struct lan9118_regs *regs;

	int
get_device_pid(char *name)
{
	union dev_reg_req rq;
	union dev_reg_rsp rp;

	rq.find.type = DEV_REG_find;
	snprintf(rq.find.name, sizeof(rq.find.name),
			"%s", name);

	if (send(DEV_REG_PID, &rq) != OK) {
		return ERR;
	} else if (recv(DEV_REG_PID, &rp) != DEV_REG_PID) {
		return ERR;
	} else if (rp.find.ret != OK) {
		return ERR;
	} else {
		return rp.find.pid;
	}
}

void
debug(char *fmt, ...)
{
	static int pid = -1;

	while (pid < 0) {
		pid = get_device_pid("serial0");
	}

	char s[MESSAGE_LEN] = "eth0: ";
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(s + strlen(s), 
			sizeof(s) - strlen(s), 
			fmt, ap);
	va_end(ap);

	send(pid, (uint8_t *) s);
	recv(pid, (uint8_t *) s);
}

void
lan9118_init(void)
{
	debug("id_rev = 0x%x\n", regs->id_rev);
	debug("rx_cfg = 0x%x\n", regs->rx_cfg);
	debug("tx_cfg = 0x%x\n", regs->tx_cfg);
}

	void
main(void)
{
	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];
	char *name = "eth0";

	size_t regs_pa, regs_len;
	union dev_reg_req drq;
	union dev_reg_rsp drp;
	uint8_t m[MESSAGE_LEN];
	int from;

	recv(0, init_m);
	regs_pa = init_m[0];
	regs_len = init_m[1];

	regs = map_addr(regs_pa, regs_len, MAP_DEV|MAP_RW);
	if (regs == nil) {
		raise();
	}

	debug("mapped from 0x%x to 0x%x\n", regs_pa, regs);

	lan9118_init();

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

	while (true) {
		from = recv(-1, m);
		send(from, m);
	}
}

