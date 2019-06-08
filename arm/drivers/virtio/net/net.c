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
#include <eth.h>
#include <net.h>

#define MTU  1200

struct virtio_net_dev {
	volatile struct virtio_device *base;

	struct virtq rx;

	size_t rx_h_buf_pa, rx_h_buf_len;
	uint8_t *rx_h_buf_va;

	size_t rx_b_buf_pa, rx_b_buf_len;
	uint8_t *rx_b_buf_va;
		
	struct virtq tx;

	size_t tx_h_buf_pa, tx_h_buf_len;
	uint8_t *tx_h_buf_va;

	size_t tx_b_buf_pa, tx_b_buf_len;
	uint8_t *tx_b_buf_va;
};

static uint8_t intr_stack[128]__attribute__((__aligned__(4)));
static void intr_handler(int irqn, void *arg)
{
	struct virtio_net_dev *dev = arg;
	uint8_t m[MESSAGE_LEN];

	((uint32_t *) m)[0] = dev->base->interrupt_status;

	dev->base->interrupt_ack = dev->base->interrupt_status;

	send(pid(), m);

	intr_exit();
}

static bool
init_rx(struct virtio_net_dev *dev)
{
	size_t index_h, index_b, i;
	struct virtq_desc *h, *b;

	dev->rx_h_buf_len = PAGE_ALIGN(dev->rx.size * sizeof(struct virtio_net_hdr));
	dev->rx_h_buf_pa = request_memory(dev->rx_h_buf_len);
	if (dev->rx_h_buf_pa == nil) {
		log(LOG_FATAL, "memory alloc failed");
		return false;
	}

	dev->rx_h_buf_va = map_addr(dev->rx_h_buf_pa,
			dev->rx_h_buf_len,
			MAP_DEV|MAP_RW);

	if (dev->rx_h_buf_va == nil) {
		log(LOG_FATAL, "memory map failed");
		return false;
	}

	dev->rx_b_buf_len = PAGE_ALIGN(dev->rx.size * MTU);
	dev->rx_b_buf_pa = request_memory(dev->rx_b_buf_len);
	if (dev->rx_b_buf_pa == nil) {
		log(LOG_FATAL, "memory alloc failed");
		return false;
	}

	dev->rx_b_buf_va = map_addr(dev->rx_b_buf_pa,
			dev->rx_b_buf_len,
			MAP_DEV|MAP_RW);

	if (dev->rx_b_buf_va == nil) {
		log(LOG_FATAL, "memory map failed");
		return false;
	}

	log(LOG_INFO, "rx head buf 0x%x mapped to 0x%x", 
			dev->rx_h_buf_pa, dev->rx_h_buf_va);
	
	log(LOG_INFO, "rx body buf 0x%x mapped to 0x%x", 
			dev->rx_b_buf_pa, dev->rx_b_buf_va);
	
	for (i = 0; i < dev->rx.size; i += 2) {
		h = virtq_get_desc(&dev->rx, &index_h);
		if (h == nil) {
			log(LOG_WARNING, "failed to get descriptor");
			return false;
		}

		b = virtq_get_desc(&dev->rx, &index_b);
		if (b == nil) {
			log(LOG_WARNING, "failed to get descriptor");
			return false;
		}

		h->addr = dev->rx_h_buf_pa + i * sizeof(struct virtio_net_hdr);
		h->len = sizeof(struct virtio_net_hdr);
		h->next = index_b;
		h->flags = VIRTQ_DESC_F_WRITE | VIRTQ_DESC_F_NEXT;

		b->addr = dev->rx_b_buf_pa + i * MTU;
		b->len = MTU;
		b->flags = VIRTQ_DESC_F_WRITE;

		virtq_push(&dev->rx, index_h);
	}

	return true;
}

static bool
init_tx(struct virtio_net_dev *dev)
{
	dev->tx_h_buf_len = PAGE_ALIGN(dev->tx.size * sizeof(struct virtio_net_hdr));
	dev->tx_h_buf_pa = request_memory(dev->tx_h_buf_len);
	if (dev->tx_h_buf_pa == nil) {
		log(LOG_FATAL, "memory alloc failed");
		return false;
	}

	dev->tx_h_buf_va = map_addr(dev->tx_h_buf_pa,
			dev->tx_h_buf_len,
			MAP_DEV|MAP_RW);

	if (dev->tx_h_buf_va == nil) {
		log(LOG_FATAL, "memory map failed");
		return false;
	}

	dev->tx_b_buf_len = PAGE_ALIGN(dev->tx.size * MTU);
	dev->tx_b_buf_pa = request_memory(dev->tx_b_buf_len);
	if (dev->tx_b_buf_pa == nil) {
		log(LOG_FATAL, "memory alloc failed");
		return false;
	}

	dev->tx_b_buf_va = map_addr(dev->tx_b_buf_pa,
			dev->tx_b_buf_len,
			MAP_DEV|MAP_RW);

	if (dev->tx_b_buf_va == nil) {
		log(LOG_FATAL, "memory map failed");
		return false;
	}

	log(LOG_INFO, "tx head buf 0x%x mapped to 0x%x", 
			dev->tx_h_buf_pa, dev->tx_h_buf_va);
	
	log(LOG_INFO, "tx body buf 0x%x mapped to 0x%x", 
			dev->tx_b_buf_pa, dev->tx_b_buf_va);
	
	return true;
}

	static void
process_rx(struct net_dev *net, struct virtq_used_item *e)
{
	struct virtio_net_dev *dev = net->arg;
	struct virtq_desc *h, *b;
	uint8_t *rx_va;
	size_t off, len;

	h = &dev->rx.desc[e->index];
	b = &dev->rx.desc[h->next];

	len = e->len - h->len;

	off = b->addr - dev->rx_b_buf_pa;
	rx_va = dev->rx_b_buf_va + off;

	net_process_pkt(net, rx_va, len);

	virtq_push(&dev->rx, e->index);
}

	static void
process_tx(struct net_dev *net, struct virtq_used_item *e)
{
	struct virtio_net_dev *dev = net->arg;
	struct virtq_desc *h, *b;
	
	h = &dev->tx.desc[e->index];
	b = &dev->tx.desc[h->next];

	virtq_free_desc(&dev->tx, b, h->next);
	virtq_free_desc(&dev->tx, h, e->index);
}

	static void
send_pkt(struct net_dev *net, uint8_t *buf, size_t len)
{
	struct virtio_net_dev *dev = net->arg;
	struct virtq_desc *h, *b;
	size_t index_h, index_b;
	struct virtio_net_hdr *vh;
	uint8_t *tx_va;

	h = virtq_get_desc(&dev->tx, &index_h);
	if (h == nil) {
		log(LOG_WARNING, "failed to get descriptor");
		return;
	}

	b = virtq_get_desc(&dev->tx, &index_b);
	if (b == nil) {
		log(LOG_WARNING, "failed to get descriptor");
		return;
	}

	h->addr = dev->tx_h_buf_pa + index_h * sizeof(struct virtio_net_hdr);
	h->len = sizeof(struct virtio_net_hdr);
	h->next = index_b;
	h->flags = VIRTQ_DESC_F_NEXT;

	b->addr = dev->tx_b_buf_pa + index_b * MTU;
	b->len = len;
	b->flags = 0;

	vh	= (void *) (dev->tx_h_buf_va + index_h * sizeof(struct virtio_net_hdr));
	vh->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
	vh->gso_type = 0;
	vh->csum_start = 0;
	vh->csum_offset = len;

	tx_va = dev->tx_b_buf_va + index_b * MTU;
	memcpy(tx_va, buf, len);

	virtq_push(&dev->tx, index_h);
}

int
claim_irq(struct virtio_net_dev *dev, size_t irqn)
{
	union proc0_req rq;
	union proc0_rsp rp;

	rq.irq_reg.type = PROC0_irq_reg_req;
	rq.irq_reg.irqn = irqn; 
	rq.irq_reg.func = &intr_handler;
	rq.irq_reg.arg = dev;
	rq.irq_reg.sp = &intr_stack[sizeof(intr_stack)];

	if (mesg(PROC0_PID, &rq, &rp) != OK || rp.irq_reg.ret != OK) {
		return ERR;
	}

	return OK;
}

	int
init_dev(struct net_dev *net, struct virtio_net_dev *dev)
{
	if (dev->base->magic != VIRTIO_DEV_MAGIC) {
		log(LOG_FATAL, "virtio register magic bad 0x%x != expected 0x%x",
				dev->base->magic, VIRTIO_DEV_MAGIC);
		return ERR;
	}

	log(LOG_INFO, "virtio version = 0x%x",
			dev->base->version);

	if (dev->base->device_id != VIRTIO_DEV_TYPE_NET) {
		log(LOG_FATAL, "virtio type bad %i", dev->base->device_id);
		return ERR;
	}

	dev->base->status = VIRTIO_ACK;
	dev->base->status |= VIRTIO_DRIVER;

	/* TODO: negotiate */

	struct virtio_net_config *config = 
		(struct virtio_net_config *) dev->base->config;

	memcpy(net->mac, config->mac, 6);

	log(LOG_INFO, "net status = %i", config->status);

	dev->base->device_features_sel = 0;
	dev->base->driver_features_sel = 0;
	dev->base->driver_features = 
		(1<<VIRTIO_NET_F_MAC) |
		(1<<VIRTIO_NET_F_STATUS) |
		(1<<VIRTIO_NET_F_CSUM);

	log(LOG_INFO, "net features = 0x%x", dev->base->device_features);

	dev->base->driver_features_sel = 1;
	dev->base->driver_features = 0;

	dev->base->status |= VIRTIO_FEATURES_OK;
	if (!(dev->base->status & VIRTIO_FEATURES_OK)) {
		log(LOG_FATAL, "virtio feature set rejected!");
		return ERR;
	}

	dev->base->page_size = PAGE_SIZE;

	if (!virtq_init(&dev->rx, dev->base, 0, 16)) {
		log(LOG_FATAL, "virtio queue init failed");
		return ERR;
	}

	if (!virtq_init(&dev->tx, dev->base, 1, 16)) {
		log(LOG_FATAL, "virtio queue init failed");
		return ERR;
	}

	dev->base->status |= VIRTIO_DRIVER_OK;

	if (dev->base->status != 0xf) {
		log(LOG_FATAL, "virtio status bad");
		return ERR;
	}

	if (!init_rx(dev)) {
		return ERR;
	}

	if (!init_tx(dev)) {
		return ERR;
	}

	return OK;
}

	int
main(void)
{
	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];
	char dev_name[MESSAGE_LEN];

	struct virtio_net_dev dev;
	struct net_dev net;

	net.arg = &dev;
	net.mtu = MTU;
	net.send_pkt = &send_pkt;

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
		log(LOG_FATAL, "virtio-net failed to map registers 0x%x 0x%x!",
				regs_pa, regs_len);
		return ERR;
	}

	log(LOG_INFO, "virtio-net on 0x%x 0x%x with irq %i",
			regs_pa, regs_len, irqn);

	dev.base = (struct virtio_device *) ((size_t) regs_va + regs_off);

	log(LOG_INFO, "virtio-blk mapped 0x%x -> 0x%x",
			regs_pa, dev.base);

	init_dev(&net, &dev);

	if (claim_irq(&dev, irqn) != OK) {
		log(LOG_FATAL, "failed to register interrupt %i", irqn);
		return ERR;
	}

	if (net_init(&net) != OK) {
		return ERR;
	}

	uint8_t m[MESSAGE_LEN];
	while (true) {
		int from = recv(-1, m);

		if (from < 0) return from;

		if (from == pid()) {
			struct virtq_used_item *e;

			e = virtq_pop(&dev.rx);
			if (e != nil) {
				process_rx(&net, e);	
			}

			e = virtq_pop(&dev.tx);
			if (e != nil) {
				process_tx(&net, e);	
			}

		} else {
			net_handle_message(&net, from, m);	
		}

	}

	return ERR;
}

