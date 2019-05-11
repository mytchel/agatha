#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <mesg.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <log.h>
#include <dev_reg.h>
#include <arm/lan9118.h>

static volatile struct lan9118_regs *regs;

void
lan9118_init(void)
{
	log(LOG_INFO, "id_rev = 0x%x", regs->id_rev);
	log(LOG_INFO, "rx_cfg = 0x%x", regs->rx_cfg);
	log(LOG_INFO, "tx_cfg = 0x%x", regs->tx_cfg);
}

	void
main(void)
{
	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];
	char dev_name[MESSAGE_LEN];

	size_t regs_pa, regs_len, irqn;
	union dev_reg_req drq;
	union dev_reg_rsp drp;
	uint8_t m[MESSAGE_LEN];
	int from;

	recv(0, init_m);
	regs_pa = init_m[0];
	regs_len = init_m[1];
	irqn = init_m[2];
	
	recv(0, dev_name);

	log_init(dev_name);

	regs = map_addr(regs_pa, regs_len, MAP_DEV|MAP_RW);
	if (regs == nil) {
		log(LOG_FATAL, "lan9118 failed to map registers!");
		exit();
	}

	log(LOG_INFO, "mapped from 0x%x to 0x%x with irq %i", regs_pa, regs, irqn);

	lan9118_init();

	drq.type = DEV_REG_register_req;
	drq.reg.pid = pid();
	snprintf(drq.reg.name, sizeof(drq.reg.name),
			"%s", dev_name);

	if (mesg(DEV_REG_PID, (uint8_t *) &drq, &drp) != OK) {
		exit();
	}

	if (drp.reg.ret != OK) {
		exit();
	}

	while (true) {
		from = recv(-1, m);
		send(from, m);
	}
}

