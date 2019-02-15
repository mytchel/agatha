#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <dev_reg.h>
#include <arm/am335x_lcd.h>

static volatile struct am335x_lcd_regs *regs;

int
get_device_pid(char *name)
{
	union dev_reg_req rq;
	union dev_reg_rsp rp;

	rq.find.type = DEV_REG_find;
	rq.find.block = true;
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

	char s[MESSAGE_LEN] = "lcd0: ";
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

	/* prm has to enable the lcd module.
TODO: make some protocol for this 
	 */
	int prm_cm_pid;
	do {
	 prm_cm_pid = get_device_pid("prm-cm0");
	} while (prm_cm_pid < 0);

	uint8_t m[MESSAGE_LEN];
	mesg(prm_cm_pid, m, m);

	debug("mapped 0x%x -> 0x%x\n", regs_pa, regs);

	uint32_t minor = regs->pid & ((1<<5)-1);
	uint32_t major = (regs->pid >> 8) & ((1<<3)-1);

	debug("version %i.%i\n", major, minor);
	debug("status 0x%x\n", regs->irqstatus_raw);

	size_t fb_size = PAGE_ALIGN(32 + 640*480+4);
	size_t fb_pa = request_memory(fb_size);
	if (fb_pa == nil) {
		debug("failed to get memory for fb\n");
		exit();
	}

	uint32_t *fb = map_addr(fb_pa, fb_size, MAP_RW|MAP_DEV);
	if (fb == nil) {
		debug("failed to map fp\n");
		exit();
	}

	debug("frame buffer of size 0x%x mapped at 0x%x\n", fb_size, fb);

	debug("clear fb\n");
	memset(fb, 0, 0x20);
	fb[0] = 0x4000;
	memset(fb + 8, 0, fb_size - 0x20);

	debug("fb cleared\n");

	uint32_t clock_div = 2;
	uint32_t burst_size = 16;

	debug("status now 0x%x\n", regs->irqstatus_raw);

	regs->clkc_enable = 7;
	regs->raster_ctrl = 0;
	regs->ctrl = (clock_div << 8) | 1;
	regs->lcddma_fb[0].base = fb_pa;
	regs->lcddma_fb[0].ceiling = fb_pa + 32+640*480*4;
	regs->lcddma_fb[1].base = fb_pa;
	regs->lcddma_fb[1].ceiling = fb_pa + 32+640*480*4;
	regs->lcddma_ctrl = (burst_size << 4);

	debug("status now 0x%x\n", regs->irqstatus_raw);

	regs->raster_timing[0] = 
		(0 << 24) | /* hbp horizontal back porch 7:0 */
		(0 << 16) | /* hfp horizontal front porch 7:0 */
		(0 << 10) | /* hswp horizontal sync pulse width */
		(40 << 4)  | /* ppllsb pixels per line lsb */
		(0 << 3);   /* pplmsb pixels per line msb */

	debug("status now 0x%x\n", regs->irqstatus_raw);

	regs->raster_timing[1] = 
		(0 << 24) | /* vbp */
		(0 << 16) | /* vfp */
		(0 << 10) | /* vsw */
		(480 << 0);   /* lpp lines per panel */

	debug("status now 0x%x\n", regs->irqstatus_raw);

	regs->raster_timing[2] = 
		(0 << 27) | /* horizontal sync width */
		(0 << 26) | /* lines per panel bit 10 */
		(0 << 25) | /* hsync/vsync pixel clock control */
		(0 << 24) | /* program hsync/vsync rise of fall */
		(0 << 23) | /* invert output */
		(0 << 22) | /* invert pixel clock */
		(0 << 21) | /* invert hsync */
		(0 << 20) | /* invert vsync */
		(0 << 16) | /* ac bias pins */
		(0 << 8)  | /* ac bias frequency */
		(0 << 4)  | /* horizontal back porch 9:8 */
		(0 << 0)  | /* horizontal front porch 9:8 */
		0xff00; /* just set the ac bias */
	
	debug("status now 0x%x\n", regs->irqstatus_raw);
	
	regs->raster_ctrl = 
		(1 << 26) | /* unpacked 24 bit */
		(1 << 25) | /* 24 bit */
		(2 << 20) | /* pal mode raw */
		(1 << 7) |  /* tft mode */
		(1 << 0);   /* enable */

	debug("status now 0x%x\n", regs->irqstatus_raw);

	uint32_t stat = regs->irqstatus_raw;
	/*
	while (regs->irqstatus_raw == stat)
		;
		*/
	debug("status now 0x%x\n", regs->irqstatus_raw);

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

	while (true) {
		recv(-1, init_m);
	}
}

