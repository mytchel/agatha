#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <dev_reg.h>
#include <log.h>
#include <arm/am33xx_prm_cm.h>

static volatile struct cm_perpll  *cm_perpll;
static volatile struct cm_device  *cm_device;
static volatile struct cm_mpu     *cm_mpu;
static volatile struct cm_wkuppll *cm_wkup;
static volatile struct cm_dpll    *cm_dpll;
static volatile struct prm_wkup   *prm_wkup;
static volatile struct prm_per    *prm_per;

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

	log_init(name);

	regs = map_addr(regs_pa, regs_len, MAP_DEV|MAP_RW);
	if (regs == nil) {
		log(LOG_FATAL, "failed to map registers!");
		exit();
	}

	log(LOG_INFO, "on pid %i mapped 0x%x -> 0x%x", pid(), regs_pa, regs);

	drq.type = DEV_REG_register;
	drq.reg.pid = pid();
	snprintf(drq.reg.name, sizeof(drq.reg.name),
			"%s", name);

	send(DEV_REG_PID, (uint8_t *) &drq);
	while (recv(DEV_REG_PID, (uint8_t *) &drp) != DEV_REG_PID)
		;

	if (drp.reg.ret != OK) {
		log(LOG_FATAL, "failed to register with dev reg!");
		exit();
	}

	cm_perpll  = regs;
	cm_wkup    = (void *) ((size_t) regs + 0x400);
	cm_dpll    = (void *) ((size_t) regs + 0x500);
	cm_mpu     = (void *) ((size_t) regs + 0x600);
	cm_device  = (void *) ((size_t) regs + 0x700);
	prm_per    = (void *) ((size_t) regs + 0xc00);
	prm_wkup   = (void *) ((size_t) regs + 0xd00);

	log(LOG_INFO, "per rst       = 0x%x", prm_per->rstctrl);
	log(LOG_INFO, "per pwrstst   = 0x%x", prm_per->pwrstst);
	log(LOG_INFO, "per pwrstctrl = 0x%x", prm_per->pwrstctrl);

	log(LOG_INFO, "per l4ls      = 0x%x", cm_perpll->l4lsclkstctrl);
	log(LOG_INFO, "per l3s       = 0x%x", cm_perpll->l3sclkstctrl);
	log(LOG_INFO, "per l3        = 0x%x", cm_perpll->l3clkstctrl);
	log(LOG_INFO, "per l4ls      = 0x%x", cm_perpll->l4lsclkctrl);

	/* need to set bit 17 of l4ls for lcd */

	log(LOG_INFO, "wkup rst       = 0x%x", prm_wkup->rstctrl);
	log(LOG_INFO, "wkup pwrstctrl = 0x%x", prm_wkup->pwrstctrl);
	log(LOG_INFO, "wkup pwrstst   = 0x%x", prm_wkup->pwrstst);
	log(LOG_INFO, "wkup rstst     = 0x%x", prm_wkup->rstst);
	
	log(LOG_INFO, "cmdevice clkout = 0x%x", cm_device->clkout_ctrl);

	log(LOG_INFO, "mpu stctrl      = 0x%x", cm_mpu->clkstctrl);
	log(LOG_INFO, "mpu clkctrl     = 0x%x", cm_mpu->clkctrl);

	log(LOG_INFO, "enable lcd");
	log(LOG_INFO, "enable lcd 0x%x", cm_perpll->lcdclkctrl);
	cm_perpll->lcdclkctrl = 0x2;
	while ((cm_perpll->lcdclkctrl >> 18) & 1)
		;

	log(LOG_INFO, "lcd enabled module");

	uint32_t temp, clksel, m, n;

#if 0
	m = 407;
	n = 97;
#elif 0
	m = 275;
	n = 71;
#else
	m = 84;
	n = 41;
/* d = 2 */
#endif

#define ST_MN_BYPASS_MASK      (1<<8)
#define ST_DPLL_CLK_MASK       1
#define CLKMOD_DPLL_EN_MASK    7
#define CLKMOD_DPLL_EN_BYPASS  4
#define CLKMOD_DPLL_EN_LOCK    7

#define CLKSEL_DPLL_M_SHIFT    8
#define CLKSEL_DPLL_M_MASK     (0x7ff << 8)
#define CLKSEL_DPLL_N_SHIFT    0
#define CLKSEL_DPLL_N_MASK     0x7f

	clksel = cm_wkup->clksel_dpll_disp;

	/* bypass dpll*/
	temp = cm_wkup->clkmod_dpll_disp;
	temp &= CLKMOD_DPLL_EN_MASK;
	temp |= CLKMOD_DPLL_EN_BYPASS;
	cm_wkup->clkmod_dpll_disp = temp;

	log(LOG_INFO, "lcd wait for bypass");
	while ((cm_wkup->idlest_dpll_disp & ST_DPLL_CLK_MASK))
		yield();

	log(LOG_INFO, "lcd setup");

	/* set m & n */

	clksel &= ~CLKSEL_DPLL_M_MASK;
	clksel |= (m << CLKSEL_DPLL_M_SHIFT) & CLKSEL_DPLL_M_MASK;
	clksel &= ~CLKSEL_DPLL_N_MASK;
	clksel |= (n << CLKSEL_DPLL_N_SHIFT) & CLKSEL_DPLL_N_MASK;

	cm_wkup->clksel_dpll_disp = clksel;

	/* post dividers. No need? */
	/* do dpll lock */

	temp = cm_wkup->clkmod_dpll_disp;
	temp &= ~CLKMOD_DPLL_EN_MASK;
	temp |= CLKMOD_DPLL_EN_LOCK;
	
	cm_wkup->clkmod_dpll_disp = temp;

	/* wait for lock */

	log(LOG_INFO, "lcd wait for lock");
	while (!(cm_wkup->idlest_dpll_disp & ST_DPLL_CLK_MASK))
		yield();

	/* use dpll_disp for pixel clk */
	cm_dpll->clklcdcpixelclk = 0;

	log(LOG_INFO, "lcd clocks enabled");

	log(LOG_INFO, "enable i2c0");
	cm_wkup->wkup_i2c0ctrl = 0x2;
	while ((cm_wkup->wkup_i2c0ctrl >> 16) & 3)
		;
	log(LOG_INFO, "enabled i2c0");

	log(LOG_INFO, "enable i2c1");
	cm_perpll->i2c1clkctrl = 0x2;
	while ((cm_perpll->i2c1clkctrl >> 16) & 3)
		;
	log(LOG_INFO, "enabled i2c1");
	
	log(LOG_INFO, "enable i2c2");
	cm_perpll->i2c2clkctrl = 0x2;
	while ((cm_perpll->i2c2clkctrl >> 16) & 3)
		;
	log(LOG_INFO, "enabled i2c2");
	
	uint8_t buf[MESSAGE_LEN];
	while (true) {
		int pid = recv(-1, buf);
		send(pid, buf);
	}
}

