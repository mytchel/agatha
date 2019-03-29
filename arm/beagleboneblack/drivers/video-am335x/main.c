#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <log.h>
#include <dev_reg.h>
#include <arm/am335x_lcd.h>
#include <i2c.h>

#define HDMI_ADDR           0x70

#define HDMI_PAGELESS       0xff

#define HDMI_CTRL_PAGE      0x00
#define HDMI_PPL_PAGE       0x02
#define HDMI_EDID_PAGE      0x09
#define HDMI_INFO_PAGE      0x10
#define HDMI_AUDIO_PAGE     0x11
#define HDMI_HDCP_OTP_PAGE  0x12
#define HDMI_GAMUT_PAGE     0x13

#define HDMI_CTRL_REV_LO_REG      0x00
#define HDMI_CTRL_REV_HI_REG      0x02
#define HDMI_REV_TDA19988       0x0331

#define HDMI_CTRL_RESET_REG       0x0a
#define HDMI_CTRL_RESET_DDC_MASK  0x02

#define HDMI_CTRL_DDC_CTRL_REG    0x0a
#define HDMI_CTRL_DDC_EN_MASK     0x02

#define HDMI_CTRL_DDC_CLK_REG     0x0c
#define HDMI_CTRL_DDC_CLK_EN_MASK 0x01

#define HDMI_CTRL_INTR_CLK_REG     0x0f
#define HDMI_CTRL_INTR_EN_GLO_MASK 0x04

#define HDMI_CTRL_INT_REG          0x11
#define HDMI_CTRL_INT_EDID_MASK    0x02



#define HDMI_EDID_DATA_REG         0x00

#define HDMI_EDID_DEV_ADDR_REG     0xfb
#define HDMI_EDID_DEV_ADDR         0xa0

#define HDMI_EDID_OFFSET_REG       0xfc
#define HDMI_EDID_OFFSET           0x00

#define HDMI_EDID_SEG_PTR_ADDR_REG 0xfc
#define HDMI_EDID_SEG_PTR_ADDR     0x00

#define HDMI_EDID_SEG_ADDR_REG     0xfe
#define HDMI_EDID_SEG_ADDR         0x00

#define HDMI_EDID_REQ_REG          0xfa
#define HDMI_EDID_REQ_READ_MASK    0x01



#define HDMI_HDCP_OTP_DDC_CLK_REG  0x9a
#define HDMI_HDCP_OTP_DDC_CLK_MASK 0x27

#define HDMI_HDCP_OTP_SOME_REG     0x9b
#define HDMI_HDCP_OTP_SOME_MASK    0x02

static volatile struct am335x_lcd_regs *regs;
static int i2c_pid;
	
static uint32_t *fb;
static uint32_t fb_pa;

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

int init_tda(void)
{
	union i2c_req i2c_rq;
	union i2c_rsp i2c_rp;
	uint32_t rev;

	do {
		i2c_pid = get_device_pid("i2c0");
	} while (i2c_pid < 0);

	i2c_rq.configure.type = I2C_configure;
	i2c_rq.configure.addr = 0x00;
	i2c_rq.configure.speed_kHz = 400;

	if (mesg(i2c_pid, &i2c_rq, &i2c_rp) != OK || i2c_rp.untyped.ret != OK) {
		log(LOG_WARNING, "failed to setup i2c");
		exit();
	}

	i2c_rq.write.type = I2C_write;
	i2c_rq.write.slave = HDMI_ADDR;
	i2c_rq.write.addr = HDMI_PAGELESS;
	i2c_rq.write.len = 1;
	i2c_rq.write.buf[0] = HDMI_CTRL_PAGE;

	if (mesg(i2c_pid, &i2c_rq, &i2c_rp) != OK || i2c_rp.untyped.ret != OK) {
		log(LOG_WARNING, "failed to set hdmi page");
		exit();
	}

	i2c_rq.read.type = I2C_read;
	i2c_rq.read.slave = HDMI_ADDR;
	i2c_rq.read.addr = HDMI_CTRL_REV_LO_REG;
	i2c_rq.read.len = 1;

	if (mesg(i2c_pid, &i2c_rq, &i2c_rp) != OK || i2c_rp.untyped.ret != OK) {
		log(LOG_WARNING, "failed to read hdmi rev");
		exit();
	}

	rev = i2c_rp.read.buf[0];

	i2c_rq.read.type = I2C_read;
	i2c_rq.read.slave = HDMI_ADDR;
	i2c_rq.read.addr = HDMI_CTRL_REV_HI_REG;
	i2c_rq.read.len = 1;

	if (mesg(i2c_pid, &i2c_rq, &i2c_rp) != OK || i2c_rp.untyped.ret != OK) {
		log(LOG_WARNING, "failed to read hdmi rev");
		exit();
	}

	rev |= ((uint32_t) i2c_rp.read.buf[0]) << 8;

	log(LOG_INFO, "TDA19988 revision = 0x%x", rev);

	if (rev != HDMI_REV_TDA19988) {
		log(LOG_WARNING, "unknown module");
		return ERR;
	}

	return OK;
}

int init_lcd(void)
{
	uint32_t clock_div = 2;
	uint32_t burst_size = 16;

	uint32_t minor = regs->pid & ((1<<5)-1);
	uint32_t major = (regs->pid >> 8) & ((1<<3)-1);

	log(LOG_INFO, "version %i.%i", major, minor);

	log(LOG_INFO, "status 0x%x", regs->irqstatus_raw);

	log(LOG_INFO, "status now 0x%x", regs->irqstatus_raw);

	regs->irqenable_clr = 0xffffffff;

	/* TODO: set up clock.
		 24.576 MHz from bbb reference?
		 24 or 26 MHz in u-boot,
		 need 25.175 MHz for the mode being set up.

		 select input with CLKSEL_LCDC_PIXEL_CLK in CM_DPLL
		 then in cm_wkup set registers 
		 CLOCKMODE_DPLL_DISP, 
		 IDLEST_DPLL_DISP,
		 CLKSEL_DPLL_DISP,
		 DIV_M2_DPLL_DISP
	 */

	regs->clkc_enable = 7;
	regs->raster_ctrl = 0;
	regs->ctrl = (clock_div << 8) | 1;
	regs->lcddma_fb[0].base = fb_pa;
	regs->lcddma_fb[0].ceiling = fb_pa + 32+640*480*4;
	regs->lcddma_fb[1].base = fb_pa;
	regs->lcddma_fb[1].ceiling = fb_pa + 32+640*480*4;
	regs->lcddma_ctrl = (burst_size << 4);

	log(LOG_INFO, "status now 0x%x", regs->irqstatus_raw);

	regs->raster_timing[0] = 
		(48 << 24) | /* hbp horizontal back porch 7:0 */
		(16 << 16) | /* hfp horizontal front porch 7:0 */
		(0  << 10) | /* hswp horizontal sync pulse width 5:0 */
		(40 << 4)  | /* ppllsb pixels per line lsb */
		(0 << 3);   /* pplmsb pixels per line msb */

	log(LOG_INFO, "status now 0x%x", regs->irqstatus_raw);

	regs->raster_timing[1] = 
		(33 << 24) | /* vbp */
		(10 << 16) | /* vfp */
		(2 << 10) | /* vsw */
		(480 << 0);   /* lpp lines per panel */

	log(LOG_INFO, "status now 0x%x", regs->irqstatus_raw);

	regs->raster_timing[2] = 
		(3 << 27) | /* horizontal sync width 9:6 */
		(0 << 26) | /* lines per panel bit 10 */
		(0 << 25) | /* hsync/vsync pixel clock control */
		(0 << 24) | /* program hsync/vsync rise or fall */
		(0 << 23) | /* invert output */
		(0 << 22) | /* invert pixel clock */
		(0 << 21) | /* invert hsync */
		(0 << 20) | /* invert vsync */
		(0 << 16) | /* ac bias pins */
		(0 << 8)  | /* ac bias frequency */
		(0 << 4)  | /* horizontal back porch 9:8 */
		(0 << 0)  | /* horizontal front porch 9:8 */
		0xff00; /* just set the ac bias */

	log(LOG_INFO, "status now 0x%x", regs->irqstatus_raw);

	regs->raster_ctrl = 
		(1 << 26) | /* unpacked 24 bit */
		(1 << 25) | /* 24 bit */
		(2 << 20) | /* pal mode raw */
		(1 << 7) |  /* tft mode */
		(1 << 0);   /* enable */

	log(LOG_INFO, "status now 0x%x", regs->irqstatus_raw);

	return OK;
}

	void
main(void)
{
	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];
	char dev_name[MESSAGE_LEN];

	size_t regs_pa, regs_len;
	union dev_reg_req drq;
	union dev_reg_rsp drp;

	recv(0, init_m);

	regs_pa = init_m[0];
	regs_len = init_m[1];

	recv(0, dev_name);

	log_init(dev_name);

	log(LOG_INFO, "on pid %i", pid());

	regs = map_addr(regs_pa, regs_len, MAP_DEV|MAP_RW);
	if (regs == nil) {
		log(LOG_WARNING, "failed to map registers!");
		exit();
	}

	log(LOG_INFO, "on pid %i mapped 0x%x -> 0x%x", pid(), regs_pa, regs);

	/* prm has to enable the lcd module.
TODO: make some protocol for this 
	 */
	int prm_cm_pid;
	do {
		prm_cm_pid = get_device_pid("prm-cm0");
	} while (prm_cm_pid < 0);

	log(LOG_INFO, "video has prm pid %i", prm_cm_pid);

	uint8_t m[MESSAGE_LEN];
	mesg(prm_cm_pid, m, m);

	size_t fb_size = 0x20 + 640*480*4;
	fb_pa = request_memory(fb_size);
	if (fb_pa == nil) {
		log(LOG_WARNING, "failed to get memory for fb");
		exit();
	}

	log(LOG_INFO, "have fb mem at 0x%x size 0x%x", fb_pa, fb_size);

	fb = map_addr(fb_pa, fb_size, MAP_RW|MAP_DEV);
	if (fb == nil) {
		log(LOG_WARNING, "failed to map frame buffer");
		exit();
	}

	log(LOG_INFO, "frame buffer of size 0x%x mapped at 0x%x", fb_size, fb);

	if (init_tda() != OK) {
		log(LOG_WARNING, "error initializing TDA19988");
		exit();
	}

	memset(fb, 0, 0x20);
	fb[0] = 0x4000;
	memset(fb + 8, 0, fb_size - 0x20);

	if (init_lcd() != OK) {
		log(LOG_WARNING, "error initialising lcd");
		exit();
	}

	drq.type = DEV_REG_register;
	drq.reg.pid = pid();
	snprintf(drq.reg.name, sizeof(drq.reg.name),
			"%s", dev_name);

	if (mesg(DEV_REG_PID, &drq, &drp) != OK || drp.reg.ret != OK) {
		log(LOG_WARNING, "failed to register with dev reg!");
		exit();
	}

	while (true) {
		recv(-1, init_m);
	}
}

