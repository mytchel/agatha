#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <dev_reg.h>
#include <arm/am33xx_prm_cm.h>

static volatile struct cm_perpll *cm_perpll_regs;
static volatile struct prm_wkup  *prm_wkup_regs;
static volatile struct prm_per   *prm_per_regs;

int
get_device_pid(char *name)
{
	union dev_reg_req rq;
	union dev_reg_rsp rp;

	rq.find.type = DEV_REG_find;
	snprintf(rq.find.name, sizeof(rq.find.name),
			"%s", name);

	if (mesg(DEV_REG_PID, &rq, &rp) != OK) {
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

	char s[MESSAGE_LEN] = "prm-cm0: ";
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(s + strlen(s),
			sizeof(s) - strlen(s),
			fmt, ap);
	va_end(ap);

	mesg(pid, (uint8_t *) s, (uint8_t *) s);
}

	void
main(void)
{
	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];
	char name[MESSAGE_LEN];

	size_t regs_pa, regs_len;
	void *regs;

	union dev_reg_req drq;
	union dev_reg_rsp drp;

	recv(0, init_m);

	regs_pa = init_m[0];
	regs_len = init_m[1];
	
	recv(0, name);

	regs = map_addr(regs_pa, regs_len, MAP_DEV|MAP_RW);
	if (regs == nil) {
		debug("failed to map registers!\n");
		exit();
	}

	drq.type = DEV_REG_register;
	drq.reg.pid = pid();
	snprintf(drq.reg.name, sizeof(drq.reg.name),
			"%s", name);

	send(DEV_REG_PID, (uint8_t *) &drq);
	while (recv(DEV_REG_PID, (uint8_t *) &drp) != DEV_REG_PID)
		;

	if (drp.reg.ret != OK) {
		debug("failed to register with dev reg!\n");
		exit();
	}

	debug("test\n");

	prm_per_regs = (void *) ((size_t) regs + 0xc00);
	debug("per           = 0x%x\n", prm_per_regs);
	debug("per rst       = 0x%x\n", prm_per_regs->rstctrl);
	debug("per pwrstst   = 0x%x\n", prm_per_regs->pwrstst);
	debug("per pwrstctrl = 0x%x\n", prm_per_regs->pwrstctrl);

	prm_wkup_regs = (void *) ((size_t) regs + 0xd00);
	debug("wkup           = 0x%x\n", prm_wkup_regs);
	debug("wkup rst       = 0x%x\n", prm_wkup_regs->rstctrl);
	debug("wkup pwrstctrl = 0x%x\n", prm_wkup_regs->pwrstctrl);
	debug("wkup pwrstst   = 0x%x\n", prm_wkup_regs->pwrstst);
	debug("wkup rstst     = 0x%x\n", prm_wkup_regs->rstst);

	cm_perpll_regs = regs;

	debug("enable lcd\n");
	debug("enable lcd 0x%x\n", cm_perpll_regs->lcdclkctrl);
	cm_perpll_regs->lcdclkctrl = 0x2;
	while ((cm_perpll_regs->lcdclkctrl >> 18) & 1)
		debug("enable lcd 0x%x\n", cm_perpll_regs->lcdclkctrl);
	debug("lcd enabled\n");

	uint8_t m[MESSAGE_LEN];
	while (true) {
		int pid = recv(-1, m);
		send(pid, m);
	}
}

