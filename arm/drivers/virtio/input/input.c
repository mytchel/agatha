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
#include <virtio-input.h>

struct virtio_input_dev {
	volatile struct virtio_device *base;

	struct virtq eventq;
	struct virtq statusq;

	size_t event_buf_pa, event_buf_len;
	uint8_t *event_buf_va;
};

static uint8_t intr_stack[128]__attribute__((__aligned__(4)));
static void intr_handler(int irqn, void *arg)
{
	struct virtio_input_dev *dev = arg;
	uint8_t m[MESSAGE_LEN];

	((uint32_t *) m)[0] = dev->base->interrupt_status;

	dev->base->interrupt_ack = dev->base->interrupt_status;

	send(pid(), m);

	intr_exit();
}

int
claim_irq(void *arg, size_t irqn)
{
	union proc0_req rq;
	union proc0_rsp rp;

	rq.irq_reg.type = PROC0_irq_reg_req;
	rq.irq_reg.irqn = irqn; 
	rq.irq_reg.func = &intr_handler;
	rq.irq_reg.arg = arg;
	rq.irq_reg.sp = &intr_stack[sizeof(intr_stack)];

	if (mesg(PROC0_PID, &rq, &rp) != OK || rp.irq_reg.ret != OK) {
		return ERR;
	}

	return OK;
}

	int
init_dev(struct virtio_input_dev *dev)
{
	if (dev->base->magic != VIRTIO_DEV_MAGIC) {
		log(LOG_FATAL, "virtio register magic bad 0x%x != expected 0x%x",
				dev->base->magic, VIRTIO_DEV_MAGIC);
		return ERR;
	}

	log(LOG_INFO, "virtio version = 0x%x",
			dev->base->version);

	if (dev->base->device_id != VIRTIO_DEV_TYPE_INPUT) {
		log(LOG_FATAL, "virtio type bad %i", dev->base->device_id);
		return ERR;
	}

	dev->base->status = VIRTIO_ACK;
	dev->base->status |= VIRTIO_DRIVER;

	/* TODO: negotiate */

	struct virtio_input_config *config = 
		(struct virtio_input_config *) dev->base->config;

	config->select = VIRTIO_INPUT_CFG_ID_NAME;
	char name[32];
	strlcpy(name, config->u.string, 32);

	log(LOG_INFO, "name = %s", name);

	config->subsel = 1;
	config->select = VIRTIO_INPUT_CFG_EV_BITS;

	log(LOG_INFO, "check events, size is %i", config->size);

	size_t i, b;
	for (i = 0; i < config->size; i++) {
		for (b = 0; b < 8; b++) {
			if (config->u.bitmap[i] & (1<<b)) {
				log(LOG_INFO, "supports event %i", i*8+b);
			}
		}
	}

	dev->base->device_features_sel = 0;
	dev->base->driver_features_sel = 0;
	dev->base->driver_features = 0;

	log(LOG_INFO, "features = 0x%x", dev->base->device_features);

	dev->base->driver_features_sel = 1;
	dev->base->driver_features = 0;

	dev->base->status |= VIRTIO_FEATURES_OK;
	if (!(dev->base->status & VIRTIO_FEATURES_OK)) {
		log(LOG_FATAL, "virtio feature set rejected!");
		return ERR;
	}

	dev->base->page_size = PAGE_SIZE;

	if (!virtq_init(&dev->eventq, dev->base, 0, 64)) {
		log(LOG_FATAL, "virtio queue init failed");
		return ERR;
	}

	if (!virtq_init(&dev->statusq, dev->base, 1, 64)) {
		log(LOG_FATAL, "virtio queue init failed");
		return ERR;
	}

	dev->base->status |= VIRTIO_DRIVER_OK;

	if (dev->base->status != 0xf) {
		log(LOG_FATAL, "virtio status bad");
		return ERR;
	}

	dev->event_buf_len = PAGE_ALIGN(dev->eventq.size * sizeof(struct virtio_input_event));
	dev->event_buf_pa = request_memory(dev->event_buf_len);
	if (dev->event_buf_pa == nil) {
		log(LOG_FATAL, "memory alloc failed");
		return ERR;
	}

	dev->event_buf_va = map_addr(dev->event_buf_pa,
			dev->event_buf_len,
			MAP_DEV|MAP_RW);

	if (dev->event_buf_va == nil) {
		log(LOG_FATAL, "memory map failed");
		return ERR;
	}

	return OK;
}

void
test(struct virtio_input_dev *dev)
{
	struct virtio_input_event *event;
		struct virtq_used_item *e;
	size_t i = 0;
	struct virtq_desc *d;
	size_t index;
	uint8_t m[MESSAGE_LEN];

	log(LOG_INFO, "start test");

	for (i = 0; i < dev->eventq.size;  i++) {
		log(LOG_INFO, "load item");

		d = virtq_get_desc(&dev->eventq, &index);
		if (d == nil) {
			log(LOG_WARNING, "failed to get descriptor");
			return ;
		}

		d->addr = dev->event_buf_pa + index * sizeof(struct virtio_input_event);
		d->len = sizeof(struct virtio_input_event);
		d->flags = VIRTQ_DESC_F_WRITE;

		virtq_push(&dev->eventq, index);
	}

	log(LOG_INFO, "process");

	while (true) {
		uint8_t m[MESSAGE_LEN];
		recv(pid(), m);

		log(LOG_INFO, "got int");

		while ((e = virtq_pop(&dev->eventq)) != nil) {
			d = &dev->eventq.desc[e->index];

			size_t off = d->addr - dev->event_buf_pa;
			event = (void *) (dev->event_buf_va + off);

			log(LOG_INFO, "got event type %i code %i value %i",
					(uint32_t) event->type, (uint32_t) event->code, (uint32_t) event->value);

			virtq_push(&dev->eventq, e->index);
		}
	}
}

	int
main(void)
{
	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];
	char dev_name[MESSAGE_LEN];

	struct virtio_input_dev dev;

	size_t regs_pa, regs_len;
	size_t regs_off, regs_va;
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
		log(LOG_FATAL, "virtio-input failed to map registers 0x%x 0x%x!",
				regs_pa, regs_len);
		return ERR;
	}

	log(LOG_INFO, "virtio-input on 0x%x 0x%x with irq %i",
			regs_pa, regs_len, irqn);

	dev.base = (struct virtio_device *) ((size_t) regs_va + regs_off);

	log(LOG_INFO, "virtio-input mapped 0x%x -> 0x%x",
			regs_pa, dev.base);

	if (init_dev(&dev) != OK) {
		return ERR;
	}

	if (claim_irq(&dev, irqn) != OK) {
		log(LOG_FATAL, "failed to register interrupt %i", irqn);
		return ERR;
	}

	test(&dev);

	return ERR;
}

