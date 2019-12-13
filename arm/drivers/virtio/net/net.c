#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <sysobj.h>
#include <arm/mmu.h>
#include <mesg.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <log.h>
#include <virtio.h>
#include <virtio-net.h>
#include <eth.h>
#include <net_dev.h>

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

static bool
init_rx(struct virtio_net_dev *dev)
{
	size_t index_h, index_b, i;
	struct virtq_desc *h, *b;

	int h_buf_cid, b_buf_cid;
	int type;

	dev->rx_h_buf_len = PAGE_ALIGN(dev->rx.size * sizeof(struct virtio_net_hdr));
	dev->rx_b_buf_len = PAGE_ALIGN(dev->rx.size * MTU);

	h_buf_cid = request_memory(dev->rx_h_buf_len, 0x1000);
	b_buf_cid = request_memory(dev->rx_b_buf_len, 0x1000);

	if (h_buf_cid < 0 || b_buf_cid < 0) {
		log(LOG_FATAL, "get memory for buffers failed");
		return false;
	}

	frame_info(h_buf_cid, &type, &dev->rx_h_buf_pa, &dev->rx_h_buf_len);
	frame_info(b_buf_cid, &type, &dev->rx_b_buf_pa, &dev->rx_b_buf_len);

	dev->rx_h_buf_va = frame_map_anywhere(h_buf_cid);
	dev->rx_b_buf_va = frame_map_anywhere(b_buf_cid);

	if (dev->rx_h_buf_va == nil || dev->rx_b_buf_va == nil) {
		log(LOG_FATAL, "map buffers failed");
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
	int h_buf_cid, b_buf_cid;
	int type;

	dev->tx_h_buf_len = PAGE_ALIGN(dev->tx.size * sizeof(struct virtio_net_hdr));
	dev->tx_b_buf_len = PAGE_ALIGN(dev->tx.size * MTU);

	h_buf_cid = request_memory(dev->tx_h_buf_len, 0x1000);
	b_buf_cid = request_memory(dev->tx_b_buf_len, 0x1000);

	if (h_buf_cid < 0 || b_buf_cid < 0) {
		log(LOG_FATAL, "get memory for buffers failed");
		return false;
	}

	frame_info(h_buf_cid, &type, &dev->tx_h_buf_pa, &dev->tx_h_buf_len);
	frame_info(b_buf_cid, &type, &dev->tx_b_buf_pa, &dev->tx_b_buf_len);

	dev->tx_h_buf_va = frame_map_anywhere(h_buf_cid);
	dev->tx_b_buf_va = frame_map_anywhere(b_buf_cid);

	if (dev->tx_h_buf_va == nil || dev->tx_b_buf_va == nil) {
		log(LOG_FATAL, "map buffers failed");
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
	char *dev_name = "virtio-net";
	struct virtio_net_dev dev;
	struct net_dev net;

	net.arg = &dev;
	net.mtu = MTU;
	net.send_pkt = &send_pkt;

	log_init(dev_name);
	log(LOG_INFO, "virtio-net get caps");

	union proc0_req prq;
	union proc0_rsp prp;
	int mount_cid, reg_cid, irq_cid;
	size_t reg_pa, reg_len, reg_off, reg_va;

	int irq_eid = kobj_alloc(OBJ_endpoint, 1);
	if (irq_eid < 0) {
		exit(ERR);
	}

	mount_cid = kcap_alloc();
	reg_cid = kcap_alloc();
	irq_cid = kcap_alloc();
	if (mount_cid < 0 || reg_cid < 0 || irq_cid < 0) {
		exit(ERR);
	}

	prq.get_resource.type = PROC0_get_resource;
	prq.get_resource.resource_type = RESOURCE_type_mount;

	mesg_cap(CID_PARENT, &prq, &prp, mount_cid);
	if (prp.get_resource.ret != OK) {
		exit(ERR);
	}

	net.mount_eid = mount_cid;

	prq.get_resource.type = PROC0_get_resource;
	prq.get_resource.resource_type = RESOURCE_type_regs;

	mesg_cap(CID_PARENT, &prq, &prp, reg_cid);
	if (prp.get_resource.ret != OK) {
		exit(ERR);
	}

	reg_pa = prp.get_resource.result.regs.pa;
	reg_len = prp.get_resource.result.regs.len;
	reg_off = reg_pa - PAGE_ALIGN_DN(reg_pa);

	log(LOG_INFO, "virtio-net reg 0x%x,0x%x 0x%x",
		reg_pa, reg_len, reg_off);

	prq.get_resource.type = PROC0_get_resource;
	prq.get_resource.resource_type = RESOURCE_type_int;

	mesg_cap(CID_PARENT, &prq, &prp, irq_cid);
	if (prp.get_resource.ret != OK) {
		exit(ERR);
	}

	log(LOG_INFO, "virtio-net got mount 0x%x, reg 0x%x, irq 0x%x",
		mount_cid, reg_cid, irq_cid);

	reg_va = (size_t) frame_map_anywhere(reg_cid);
	if (reg_va == nil) {
		exit(ERR);
	}

	strlcpy(net.name, dev_name, sizeof(net.name));

	dev.base = (struct virtio_device *) ((size_t) reg_va + reg_off);

	init_dev(&net, &dev);

	if (intr_connect(irq_cid, irq_eid, 0x1) != OK) {
		exit(ERR);
	}

	if (net_init(&net) != OK) {
		return ERR;
	}

	int cap = kcap_alloc();

	uint8_t m[MESSAGE_LEN];
	while (true) {
		int eid, from;

		eid = recv_cap(EID_ANY, &from, m, cap);
		if (eid < 0) continue;

		if (from == PID_SIGNAL) {
			if (eid == irq_eid) {
				dev.base->interrupt_ack = dev.base->interrupt_status;

				intr_ack(irq_cid);
				
				log(LOG_INFO, "irq handled");

				struct virtq_used_item *e;

				e = virtq_pop(&dev.rx);
				if (e != nil) {
					log(LOG_INFO, "process rx");
					process_rx(&net, e);
				} 

				e = virtq_pop(&dev.tx);
				if (e != nil) {
					log(LOG_INFO, "process tx");
					process_tx(&net, e);	
				}
			}
		} else {
			net_handle_message(&net, eid, from, m, cap);
		}

	}

	return ERR;
}

