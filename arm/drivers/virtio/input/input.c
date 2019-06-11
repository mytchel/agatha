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
#include <evdev.h>
#include <evdev_codes.h>

struct virtio_input_dev {
	volatile struct virtio_device *base;

	struct virtq eventq;
	struct virtq statusq;

	size_t event_buf_pa, event_buf_len;
	uint8_t *event_buf_va;

	int connected_pid;
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
register_dev(struct virtio_input_dev *dev, char *name)
{
	union dev_reg_req drq;
	union dev_reg_rsp drp;
	
	drq.type = DEV_REG_register_req;
	drq.reg.pid = pid();
	snprintf(drq.reg.name, sizeof(drq.reg.name),
			"%s", name);

	if (mesg(DEV_REG_PID, &drq, &drp) != OK) {
	 return ERR;
	} else {
		return drp.reg.ret; 
	}
}

	int
init_dev(struct virtio_input_dev *dev)
{
	struct virtio_input_config *config;
	struct virtq_desc *d;
	size_t index;
	size_t i;

	dev->connected_pid = -1;

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

	config = (struct virtio_input_config *) dev->base->config;

	config->select = VIRTIO_INPUT_CFG_ID_NAME;

	char name[32];
	strlcpy(name, config->u.string, 32);

	log(LOG_INFO, "name = %s", name);

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

	for (i = 0; i < dev->eventq.size;  i++) {
		d = virtq_get_desc(&dev->eventq, &index);
		if (d == nil) {
			log(LOG_WARNING, "failed to get descriptor");
			return ERR;
		}

		d->addr = dev->event_buf_pa + index * sizeof(struct virtio_input_event);
		d->len = sizeof(struct virtio_input_event);
		d->flags = VIRTQ_DESC_F_WRITE;

		virtq_push(&dev->eventq, index);
	}

	return OK;
}

void
handle_event(struct virtio_input_dev *dev,
		struct virtio_input_event *e)
{
	union evdev_msg m;

	if (e->type == EV_SYN) {
		return;
	}

	if (dev->connected_pid == -1) {
		return;
	}

	m.type = EVDEV_event_msg;
	m.event.event_type = e->type;
	m.event.code       = e->code;
	m.event.value      = e->value;

	send(dev->connected_pid, &m);
}

void
handle_connect_req(struct virtio_input_dev *dev, 
		int from, union evdev_req *rq)
{
	union evdev_rsp rp;

	rp.connect.type = EVDEV_connect_rsp;

	if (dev->connected_pid == -1) {
		rp.connect.ret = OK;
		dev->connected_pid = from;
	} else {
		rp.connect.ret = ERR;
	}

	send(from, &rp);
}

void
handle_event_msg(struct virtio_input_dev *dev, 
		int from, union evdev_msg *m)
{
	/* TODO */
}

void
main_loop(struct virtio_input_dev *dev)
{
	struct virtio_input_event *event;
	struct virtq_used_item *e;
	struct virtq_desc *d;
	uint8_t m[MESSAGE_LEN];
	int from;

	while (true) {
		from = recv(-1, m);

		if (from == pid()) {
			while ((e = virtq_pop(&dev->eventq)) != nil) {
				d = &dev->eventq.desc[e->index];

				size_t off = d->addr - dev->event_buf_pa;
				event = (void *) (dev->event_buf_va + off);

				handle_event(dev, event);

				virtq_push(&dev->eventq, e->index);
			}
		} else {
			uint32_t type = *((uint32_t *) m);

			switch (type) {
				case EVDEV_connect_req:
					handle_connect_req(dev, from, (void *) m);
					break;

				case EVDEV_event_msg:
					handle_event_msg(dev, from, (void *) m);
					break;
			}
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

	register_dev(&dev, dev_name);

	main_loop(&dev);

	return ERR;
}

