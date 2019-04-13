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
#include <arm/pl111.h>
#include <sdmmc.h>

static size_t width = 640;
static size_t height = 480;
	
static size_t fb_size, fb_pa;
static uint32_t *fb;

static void intr_handler(int irqn, void *arg)
{
	uint8_t m[MESSAGE_LEN];
	volatile struct pl111_regs *regs = arg;

	regs->imsc &= ~regs->ris;

	send(pid(), m);

	intr_exit();
}

	static int 
pl111_init(volatile struct pl111_regs *regs)
{
	regs->imsc = 0;

	regs->upbase = fb_pa;

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
		(1 <<  0);  /* enable */

	return 0;
}

	void
main(void)
{
	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];
	static uint32_t intr_stack[64];
	char dev_name[MESSAGE_LEN];

	volatile struct pl111_regs *regs;
	size_t regs_pa, regs_len, irqn;
	union proc0_req rq;
	union proc0_rsp rp;
	int ret;

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
	rq.irq_reg.arg = regs;
	rq.irq_reg.sp = &intr_stack[sizeof(intr_stack)];

	if (mesg(PROC0_PID, &rq, &rp) != OK || rp.irq_reg.ret != OK) {
		log(LOG_FATAL, "failed to register interrupt %i", irqn);
		exit();
	}

	log(LOG_INFO, "pl111 mapped 0x%x -> 0x%x with irq %i", 
			regs_pa, regs, irqn);

	fb_size = 4 * (width * height);
	fb_pa = request_memory(fb_size);
	if (fb_pa == nil) {
		log(LOG_FATAL, "failed to get memory for fb");
		exit();
	}

	fb = map_addr(fb_pa, fb_size, MAP_RW|MAP_DEV);
	if (fb == nil) {
		log(LOG_FATAL, "failed to map frame buffer");
		exit();
	}

	memset(fb, 0, fb_size);

	int x, y;
	for (x = 32; x < 200; x++) {
		for (y = 43; y < 100; y++) {
			fb[y * width + x] = 0x00fa00c0;
		}
	}

	ret = pl111_init(regs);
	if (ret != OK) {
		log(LOG_FATAL, "pl111 init failed!");
		exit();
	}

	int i;

	for (i = 0; i < 0xfffffff; i++)
		;

	for (x = 92; x < 150; x++) {
		for (y = 13; y < 200; y++) {
			fb[y * width + x] = 0x000000c0;
		}
	}

	while (true) {
		uint8_t m[MESSAGE_LEN];
		
		regs->imsc = 0x1e;
		log(LOG_INFO, "wait for irq, status = 0x%x", regs->ris);

		recv(pid(), m);

		log(LOG_INFO, "got interrupt 0x%x", regs->ris);
		log(LOG_INFO, "masked interrupt 0x%x", regs->mis);
	}	
}

