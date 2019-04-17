#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <arm/mmu.h>
#include <mesg.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <log.h>
#include <dev_reg.h>
#include <arm/pl111.h>
#include <video.h>

static size_t width = 640;
static size_t height = 480;
static size_t fb_size = 0;
static bool frame_drawing = false;
static bool frame_waiting = false;
static size_t fb_pa_ind = 0;
static size_t fb_pa[2];
static int connected_pid = -1;
	
static volatile struct pl111_regs *regs;

static void intr_handler(int irqn, void *arg)
{
	uint8_t m[MESSAGE_LEN];
	volatile struct pl111_regs *regs = arg;

	regs->imsc &= ~regs->mis;

	send(pid(), m);

	intr_exit();
}

	static int 
pl111_init(void)
{
	regs->imsc = 0;

	/* There are other fields that will be needed for real displays
		 but qemu does not use them. */

	regs->timing[0] = 
		(((width / 4) - 4) & 0xfc);  /* pixels per line lsb 16 * (ppl + 1) */

	regs->timing[1] = 
		((height - 1) <<  0);  /* lpp lines per panel (lpp + 1) */

	regs->timing[2] = 0;
	regs->timing[2] = 0;

	regs->control = 
		(0 << 16) | /* dma fifo watermark */
		(0 << 12) | /* generate interrupt at */
		(1 << 11) | /* lcd power enable */
		(0 << 10) | /* endianness in pixel */
		(0 <<  9) | /* endianness byte order */
		(0 <<  8) | /* rgb or bgr */
		(0 <<  7) | /* dual panel enable */
		(1 <<  6) | /* mono / color */
		(1 <<  5) | /* tft */
		(0 <<  4) | /* mono again (no meaning in tft) */
		(5 <<  1) | /* bpp (24bpp)*/
		(0 <<  0);  /* disable */

	regs->imsc = CLCD_INT_ERROR | CLCD_INT_BASE;

	return 0;
}

static void
handle_connect(int from, union video_req *rq)
{
	union video_rsp rp;

	connected_pid = from;

	rp.connect.type = VIDEO_connect;
	rp.connect.frame_pa = fb_pa[(fb_pa_ind + 1) % 2];
	rp.connect.frame_size = fb_size;

	rp.connect.ret = give_addr(from, fb_pa[(fb_pa_ind + 1) % 2], fb_size);
	if (rp.connect.ret != OK) {
		send(from, &rp);
		return;
	}

	rp.connect.width = width;
	rp.connect.height = height;

	frame_drawing = true;
	regs->upbase = fb_pa[fb_pa_ind];

	/* enable */
	regs->control |= 1;

	send(from, &rp);
}

static void
frame_update(void)
{
	union video_rsp rp;

	fb_pa_ind = (fb_pa_ind + 1) % 2;
	regs->upbase = fb_pa[fb_pa_ind];

	rp.update.type = VIDEO_update;
	rp.update.frame_pa = fb_pa[(fb_pa_ind + 1) % 2];
	rp.update.frame_size = fb_size;

	rp.update.ret = give_addr(connected_pid, fb_pa[(fb_pa_ind + 1) % 2], fb_size);
	send(connected_pid, &rp);
}

	static void
handle_update(int from, union video_req *rq)
{
	union video_rsp rp;

	rp.update.type = VIDEO_update;

	if (rq->update.frame_pa != fb_pa[(fb_pa_ind + 1) % 2]) {
		rp.update.ret = ERR;
		send(from, &rp);
		return;
	}

	/* update */
	if (!frame_drawing) {
		frame_update();
	} else {
		frame_waiting = true;
	}
}

	void
main(void)
{
	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];
	static uint32_t intr_stack[64];
	char dev_name[MESSAGE_LEN];

	size_t regs_pa, regs_len, irqn;
	uint8_t m[MESSAGE_LEN];
	union proc0_req rq;
	union proc0_rsp rp;
	int ret, from;
	uint32_t *fb;

	recv(0, init_m);
	regs_pa = init_m[0];
	regs_len = init_m[1];
	irqn = init_m[2];

	recv(0, dev_name);

	log_init(dev_name);

	regs = map_addr(regs_pa, regs_len, MAP_DEV|MAP_RW);
	if (regs == nil) {
		log(LOG_FATAL, "pl111 failed to map registers!");
		exit();
	}

	rq.irq_reg.type = PROC0_irq_reg;
	rq.irq_reg.irqn = irqn;
	rq.irq_reg.func = &intr_handler;
	rq.irq_reg.arg = (void *) regs;
	rq.irq_reg.sp = &intr_stack[sizeof(intr_stack)];

	if (mesg(PROC0_PID, &rq, &rp) != OK || rp.irq_reg.ret != OK) {
		log(LOG_FATAL, "failed to register interrupt %i", irqn);
		exit();
	}

	log(LOG_INFO, "pl111 mapped 0x%x -> 0x%x with irq %i", 
			regs_pa, regs, irqn);

	fb_size = PAGE_ALIGN(4 * (width * height));

	fb_pa[0] = request_memory(fb_size);
	fb_pa[1] = request_memory(fb_size);

	if (fb_pa[0] == nil || fb_pa[1] == nil) {
		log(LOG_FATAL, "failed to get memory for fb");
		exit();
	}

	fb = map_addr(fb_pa[fb_pa_ind], fb_size, MAP_RW|MAP_DEV);
	if (fb == nil) {
		log(LOG_FATAL, "failed to map frame buffer");
		exit();
	}

	memset(fb, 0, fb_size);

	unmap_addr(fb, fb_size);

	ret = pl111_init();
	if (ret != OK) {
		log(LOG_FATAL, "pl111 init failed!");
		exit();
	}

	union dev_reg_req drq;
	union dev_reg_rsp drp;

	drq.type = DEV_REG_register;
	drq.reg.pid = pid();
	memcpy(drq.reg.name, dev_name, sizeof(drq.reg.name));

	if (mesg(DEV_REG_PID, &drq, &drp) != OK) {
		log(LOG_FATAL, "failed to message dev reg at pid %i", DEV_REG_PID);
		exit();
	}

	if (drp.reg.ret != OK) {
		log(LOG_FATAL, "failed to register to dev reg");
		exit();
	}

	while (true) {
		from = recv(-1, m);

		if (from == pid()) {
			frame_drawing = false;

			if (frame_waiting) {
				frame_waiting = false;
				frame_drawing = true;
				frame_update();
			}

			regs->icr = regs->ris;
			regs->imsc = CLCD_INT_ERROR | CLCD_INT_BASE;

		} else if (connected_pid == -1 || from == connected_pid) {
			union video_req *rq = (void *) m;

			switch (rq->type) {
				case VIDEO_connect:
					handle_connect(from, rq);
					break;

				case VIDEO_update:
					handle_update(from, rq);
					break;
			}
		}
	}	
}

