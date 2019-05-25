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
#include <virtio.h>
#include "../virtq.h"

struct device {
	volatile struct virtio_device *dev;
	struct virtq rx, tx;

	size_t buffer_pa, buffer_len;
	void *buffer_va;
};

static uint8_t intr_stack[128]__attribute__((__aligned__(4)));
static void intr_handler(int irqn, void *arg)
{
	struct device *dev = arg;
	uint8_t m[MESSAGE_LEN];

	((uint32_t *) m)[0] = dev->dev->interrupt_status;

	dev->dev->interrupt_ack = dev->dev->interrupt_status;

	send(pid(), m);

	intr_exit();
}

static void
process_rx(struct device *dev, struct virtq_used_item *e)
{
	log(LOG_INFO, "process rx index %i len %i", e->index, e->len);

	struct virtq_desc *d = &dev->rx.desc[e->index];
	log(LOG_INFO, "len of buffer given is %i", d->len);

	size_t off = d->addr - dev->buffer_pa;

	uint8_t *buf = ((uint8_t *) dev->buffer_va) + off;

	char s[33];
	int i = 0;

	while (i < e->len) {
		int j;
		s[0] = 0;
		for (j = 0; j < 8; j++) {
			uint8_t v = buf[i+j];
			uint8_t l = v & 0xf;
			uint8_t h = (v >> 4) & 0xf;

			snprintf(s+j*2, 3, "%x%x", h, l);
		}

		log(LOG_INFO, "p%x %s",
				i, s);

		i += j;
	}
	
	log(LOG_INFO, "load next rx %i", e->index);
	virtq_push(&dev->rx, e->index);
}

	static void
process_tx(struct device *dev, struct virtq_used_item *e)
{
	log(LOG_INFO, "process tx index %i len %i", e->index, e->len);
}

	int
main(void)
{
	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];
	char dev_name[MESSAGE_LEN];

	struct device dev;

	size_t regs_pa, regs_len;
	size_t regs_va, regs_off;
	size_t irqn;

	recv(0, init_m);
	regs_pa = init_m[0];
	regs_len = init_m[1];
	irqn = init_m[2];

	recv(0, dev_name);

	log_init(dev_name);

	regs_off = regs_pa - PAGE_ALIGN_DN(regs_pa);

	regs_va = (size_t) map_addr(PAGE_ALIGN_DN(regs_pa), 
			PAGE_ALIGN(regs_len), 
			MAP_DEV|MAP_RW);

	if (regs_va == nil) {
		log(LOG_FATAL, "virtio-net failed to map registers 0x%x 0x%x!",
				regs_pa, regs_len);
		exit(ERR);
	}

	log(LOG_INFO, "virtio-net on 0x%x 0x%x with irq %i",
			regs_pa, regs_len, irqn);

	dev.dev = (struct virtio_device *) ((size_t) regs_va + regs_off);

	log(LOG_INFO, "virtio-blk mapped 0x%x -> 0x%x",
			regs_pa, dev);

	if (dev.dev->magic != VIRTIO_DEV_MAGIC) {
		log(LOG_FATAL, "virtio register magic bad 0x%x != expected 0x%x",
				dev.dev->magic, VIRTIO_DEV_MAGIC);
		exit(ERR);
	}

	log(LOG_INFO, "virtio magic = 0x%x",
			dev.dev->magic);

	log(LOG_INFO, "virtio version = 0x%x",
			dev.dev->version);

	if (dev.dev->device_id != VIRTIO_DEV_TYPE_NET) {
		log(LOG_FATAL, "virtio type bad %i", dev.dev->device_id);
		exit(ERR);
	}

	dev.dev->status = VIRTIO_ACK;
	dev.dev->status |= VIRTIO_DRIVER;

	/* TODO: negotiate */

	struct virtio_net_config *config = 
		(struct virtio_net_config *) dev.dev->config;

	int i;
	for (i = 0; i < 6; i++) {
		log(LOG_INFO, "mac %i = %x", i, config->mac[i]);
	}

	log(LOG_INFO, "net status = %i", config->status);

	dev.dev->device_features_sel = 0;
	dev.dev->driver_features_sel = 0;
	dev.dev->driver_features = 
		(1<<VIRTIO_NET_F_MAC) |
		(1<<VIRTIO_NET_F_STATUS);

	log(LOG_INFO, "net features = 0x%x", dev.dev->device_features);

	dev.dev->driver_features_sel = 1;
	dev.dev->driver_features = 0;

	dev.dev->status |= VIRTIO_FEATURES_OK;
	if (!(dev.dev->status & VIRTIO_FEATURES_OK)) {
		log(LOG_FATAL, "virtio feature set rejected!");
		exit(ERR);
	}

	dev.dev->page_size = PAGE_SIZE;

	if (!virtq_init(&dev.rx, dev.dev, 0, 16)) {
		log(LOG_FATAL, "virtio queue init failed");
		exit(ERR);
	}

	if (!virtq_init(&dev.tx, dev.dev, 1, 16)) {
		log(LOG_FATAL, "virtio queue init failed");
		exit(ERR);
	}

	dev.dev->status |= VIRTIO_DRIVER_OK;

	if (dev.dev->status != 0xf) {
		log(LOG_FATAL, "virtio status bad");
		exit(ERR);
	}

	union proc0_req rq;
	union proc0_rsp rp;
	rq.irq_reg.type = PROC0_irq_reg_req;
	rq.irq_reg.irqn = irqn; 
	rq.irq_reg.func = &intr_handler;
	rq.irq_reg.arg = &dev;
	rq.irq_reg.sp = &intr_stack[sizeof(intr_stack)];

	if (mesg(PROC0_PID, &rq, &rp) != OK || rp.irq_reg.ret != OK) {
		log(LOG_FATAL, "failed to register interrupt %i : %i", 
				irqn, rp.irq_reg.ret);
		exit(ERR);
	}

	dev.buffer_len = PAGE_SIZE;
	dev.buffer_pa = request_memory(dev.buffer_len);
	if (dev.buffer_pa == nil) {
		log(LOG_FATAL, "memory alloc failed");
		exit(ERR);
	}

	dev.buffer_va = map_addr(dev.buffer_pa,
			dev.buffer_len,
			MAP_DEV|MAP_RW);

	if (dev.buffer_va == nil) {
		log(LOG_FATAL, "memory alloc failed");
		exit(ERR);
	}

	log(LOG_INFO, "mapped buffer from 0x%x to 0x%x",
			dev.buffer_pa, dev.buffer_va);

	struct virtq_desc *d;
	size_t index;

	d = virtq_get_desc(&dev.rx, &index);
	d->addr = dev.buffer_pa;
	d->len = 256;
	d->flags = VIRTQ_DESC_F_WRITE;

	virtq_push(&dev.rx, index);

	d = virtq_get_desc(&dev.rx, &index);
	d->addr = dev.buffer_pa + 256;
	d->len = 256;
	d->flags = VIRTQ_DESC_F_WRITE;

	virtq_push(&dev.rx, index);

	d = virtq_get_desc(&dev.rx, &index);
	d->addr = dev.buffer_pa + 512;
	d->len = 256;
	d->flags = VIRTQ_DESC_F_WRITE;

	virtq_push(&dev.rx, index);

	uint8_t m[MESSAGE_LEN];
	while (true) {
		int from = recv(-1, m);

		log(LOG_INFO, "recv returned");
		if (from < 0) return from;

		if (from == pid()) {
			log(LOG_INFO, "got interrupt 0x%x", *((uint32_t *) m));

			struct virtq_used_item *e;

			e = virtq_pop(&dev.rx);
			if (e != nil) {
				process_rx(&dev, e);	
			}

			e = virtq_pop(&dev.tx);
			if (e != nil) {
				process_tx(&dev, e);	
			}

		} else {
			switch (((uint32_t *) m)[0]) {
			}
		}

	}

	return ERR;
}

