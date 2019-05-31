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
#include "../virtq.h"

#define PACKET_MAX_LEN  1200

const uint8_t broadcast_mac[6] = { 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff 
}; 

struct device {
	volatile struct virtio_device *dev;

	struct virtq rx;
	size_t rx_buf_pa, rx_buf_len;
	uint8_t *rx_buf_va;
	
	struct virtq tx;
	size_t tx_buf_pa, tx_buf_len;
	uint8_t *tx_buf_va;

	uint8_t mac[6];
	uint8_t ipv4[4];
};

static void
print_mac(char *s, uint8_t *mac)
{
	int i;

	for (i = 0; i < 6; i++) {
		uint8_t l, h;
		
		l = mac[i] & 0xf;
		h = (mac[i] >> 4) & 0xf;
	
		if (h < 0xa) s[i*3] = h + '0';
		else s[i*3] = h - 0xa + 'a';

		if (l < 0xa) s[i*3+1] = l + '0';
		else s[i*3+1] = l - 0xa + 'a';

		s[i*3+2] = ':';
	}

	s[5*3+2] = 0;
}

static void
dump_hex_block(uint8_t *buf, size_t len)
{
	char s[33];
	size_t i = 0;
	while (i < len) {
		int j;
		s[0] = 0;
		for (j = 0; j < 8 && i + j < len; j++) {
			uint8_t v = buf[i+j];
			uint8_t l = v & 0xf;
			uint8_t h = (v >> 4) & 0xf;

			if (h < 0xa) s[j*2] = h + '0';
			else s[j*2] = h - 0xa + 'a';

			if (l < 0xa) s[j*2+1] = l + '0';
			else s[j*2+1] = l - 0xa + 'a';
		}

		s[j*2] = 0;

		log(LOG_INFO, "p%x %s", i, s);

		i += j;
	}
}

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

static bool
init_rx(struct device *dev)
{
	size_t index_h, index_b, i;
	struct virtq_desc *h, *b;

	dev->rx_buf_len = PAGE_ALIGN(dev->rx.size * (sizeof(struct virtio_net_hdr) + PACKET_MAX_LEN));

	dev->rx_buf_pa = request_memory(dev->rx_buf_len);

	if (dev->rx_buf_pa == nil) {
		log(LOG_FATAL, "memory alloc failed");
		return false;
	}

	dev->rx_buf_va = map_addr(dev->rx_buf_pa,
			dev->rx_buf_len,
			MAP_DEV|MAP_RW);

	if (dev->rx_buf_va == nil) {
		log(LOG_FATAL, "memory map failed");
		return false;
	}

	log(LOG_INFO, "rx buf 0x%x mapped to 0x%x", 
			dev->rx_buf_pa, dev->rx_buf_va);
	
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

		h->addr = dev->rx_buf_pa + i * (sizeof(struct virtio_net_hdr) + PACKET_MAX_LEN);
		h->len = sizeof(struct virtio_net_hdr);
		h->next = index_b;
		h->flags = VIRTQ_DESC_F_WRITE | VIRTQ_DESC_F_NEXT;

		b->addr = h->addr + sizeof(struct virtio_net_hdr);
		b->len = PACKET_MAX_LEN;
		b->flags = VIRTQ_DESC_F_WRITE;

		virtq_push(&dev->rx, index_h);
	}

	return true;
}

static bool
init_tx(struct device *dev)
{
	dev->tx_buf_len = PAGE_ALIGN(dev->tx.size * (sizeof(struct virtio_net_hdr) + PACKET_MAX_LEN));

	dev->tx_buf_pa = request_memory(dev->tx_buf_len);

	if (dev->tx_buf_pa == nil) {
		log(LOG_FATAL, "memory alloc failed");
		return false;
	}

	dev->tx_buf_va = map_addr(dev->tx_buf_pa,
			dev->tx_buf_len,
			MAP_DEV|MAP_RW);

	if (dev->tx_buf_va == nil) {
		log(LOG_FATAL, "memory map failed");
		return false;
	}

	log(LOG_INFO, "tx buf 0x%x mapped to 0x%x", 
			dev->tx_buf_pa, dev->tx_buf_va);
	
	return true;
}

	static void 
test_respond_arp(struct device *dev, uint8_t *src_mac, uint8_t *src_ipv4);

	static void
process_rx(struct device *dev, struct virtq_used_item *e)
{
	struct virtq_desc *h, *b;
	uint8_t *buf;
	size_t off, len;

	h = &dev->rx.desc[e->index];
	b = &dev->rx.desc[h->next];
	
	len = e->len - h->len;

	log(LOG_INFO, "process rx len %i", e->len, len);

	off = b->addr - dev->rx_buf_pa;
	buf = dev->rx_buf_va + off;

	struct eth_hdr *hdr = (void *) buf;

	uint32_t type = hdr->tol.type[0] << 8 | hdr->tol.type[1];

	char dst[18], src[18];

	print_mac(dst, hdr->dst);
	print_mac(src, hdr->src);

	log(LOG_INFO, "from %s", src);
	log(LOG_INFO, "to   %s", dst);
	log(LOG_INFO, "type 0x%x", type);

	bool dump = false;

	if (type == 0x0806) {
		log(LOG_INFO, "have arp packet!");

		if (memcmp(hdr->dst, broadcast_mac, 6)) {
			log(LOG_INFO, "broadcast arp");
			
			uint8_t *src_ipv4 = buf + sizeof(struct eth_hdr) + 14;
			uint8_t *dst_ipv4 = buf + sizeof(struct eth_hdr) + 24;

			log(LOG_INFO, "from    for %i.%i.%i.%i",
					src_ipv4[0], src_ipv4[1], src_ipv4[2], src_ipv4[3]);

			log(LOG_INFO, "looking for %i.%i.%i.%i",
					dst_ipv4[0], dst_ipv4[1], dst_ipv4[2], dst_ipv4[3]);

			if (memcmp(dst_ipv4, dev->ipv4, 4)) {
				log(LOG_WARNING, "asking about us!!!");

				test_respond_arp(dev, hdr->src, src_ipv4);
		
				dump = true;
			}
		}
	}

	if (memcmp(hdr->dst, dev->mac, 6)) {
		log(LOG_WARNING, "this packet is for us!!!!");
		dump = true;
	}

	if (dump) {
		log(LOG_INFO, "packet:");
		dump_hex_block(buf, len);
	}

	virtq_push(&dev->rx, e->index);
}

	static void
process_tx(struct device *dev, struct virtq_used_item *e)
{
	log(LOG_INFO, "process tx index %i len %i", e->index, e->len);
}

	static int
send_pkt(struct device *dev, uint8_t *pkt, size_t len)
{
	struct virtq_desc *d;
	size_t index, off;
	uint8_t *buf;

	log(LOG_INFO, "prepare pkt with len %i", len);
	log(LOG_INFO, "tx buf mapped to 0x%x", dev->tx_buf_va);

	d = virtq_get_desc(&dev->tx, &index);
	if (d == nil) {
		log(LOG_WARNING, "failed to get descriptor");
		return ERR;
	}

	d->addr = dev->tx_buf_pa + index * (sizeof(struct virtio_net_hdr) + PACKET_MAX_LEN);
	d->flags = 0;

	off = d->addr - dev->tx_buf_pa;
	buf = dev->tx_buf_va + off;

	memcpy(&buf[sizeof(struct virtio_net_hdr)], pkt, len);

	if (len < 64) {
		log(LOG_INFO, "zero padding bytes %i to %i bytes", 
				len, 64);

		memset(buf + sizeof(struct virtio_net_hdr) + len, 
				0, 64 - len);

		len = 64;
	}

	struct virtio_net_hdr *h = (void *) buf;
	h->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
	h->gso_type = 0;
	h->csum_start = 0;
	h->csum_offset = len;
	
	d->len = sizeof(struct virtio_net_hdr) + len;

	log(LOG_INFO, "push");

	dump_hex_block(buf + sizeof(struct virtio_net_hdr), len);

	virtq_push(&dev->tx, index);

	return OK;	
}

static void 
test_respond_arp(struct device *dev, uint8_t *src_mac, uint8_t *src_ipv4)
{
	struct eth_hdr *hdr;
	uint8_t pkt[64], *bdy;

	log(LOG_INFO, "building arp response packet");

	hdr = (void *) pkt;
	bdy = pkt + sizeof(struct eth_hdr);

	memcpy(hdr->dst, src_mac, 6);
	memcpy(hdr->src, dev->mac, 6);
	hdr->tol.type[0] = 0x08;
	hdr->tol.type[1] = 0x06;

	/* hardware type (ethernet 1) */
	bdy[0] = 0;
	bdy[1] = 1;
	/* protocol type (ipv4 0x0800) */
	bdy[2] = 0x08;
	bdy[3] = 0x00;
	/* len (eth = 6, ip = 4)*/
	bdy[4] = 6;
	bdy[5] = 4;
	/* operation (1 request, 2 reply) */
	bdy[6] = 0;
	bdy[7] = 2;

	memcpy(&bdy[8], dev->mac, 6);
	memcpy(&bdy[14], dev->ipv4, 4);
	memcpy(&bdy[18], src_mac, 6);
	memcpy(&bdy[24], src_ipv4, 4);

	if (send_pkt(dev, pkt, sizeof(struct eth_hdr) + 28) != OK) {
		log(LOG_FATAL, "error sending arp test");
	}

}

	static void
test(struct device *dev)
{
	struct eth_hdr *hdr;
	uint8_t pkt[64], *bdy;

	log(LOG_INFO, "building test packet");

	hdr = (void *) pkt;
	bdy = pkt + sizeof(struct eth_hdr);

	/*uint8_t dst[6] = { 0x10, 0x13, 0x31, 0xc7, 0xfc, 0xc0 };*/
	uint8_t dst[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	uint8_t dst_ipv4[4] = { 192, 168, 10, 1 };

	memcpy(hdr->dst, dst, 6);
	memcpy(hdr->src, dev->mac, 6);
	hdr->tol.type[0] = 0x08;
	hdr->tol.type[1] = 0x06;

	/* hardware type (ethernet 1) */
	bdy[0] = 0;
	bdy[1] = 1;
	/* protocol type (ipv4 0x0800) */
	bdy[2] = 0x08;
	bdy[3] = 0x00;
	/* len (eth = 6, ip = 4)*/
	bdy[4] = 6;
	bdy[5] = 4;
	/* operation (1 request, 2 reply) */
	bdy[6] = 0;
	bdy[7] = 1;

	memcpy(&bdy[8], dev->mac, 6);
	memcpy(&bdy[14], dev->ipv4, 4);
	memset(&bdy[18], 0, 6);
	memcpy(&bdy[24], dst_ipv4, 4);

	if (send_pkt(dev, pkt, sizeof(struct eth_hdr) + 28) != OK) {
		log(LOG_FATAL, "error sending arp test");
	}
}

	static void
handle_message(int from, uint8_t *m)
{
	uint32_t type = *((uint32_t *) m);

	switch (type) {
		default:
			break;
	}
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

	memcpy(dev.mac, config->mac, 6);

	char mac_str[18];
	print_mac(mac_str, dev.mac);
	log(LOG_INFO, "mac = %s", mac_str);

	log(LOG_INFO, "net status = %i", config->status);

	dev.dev->device_features_sel = 0;
	dev.dev->driver_features_sel = 0;
	dev.dev->driver_features = 
		(1<<VIRTIO_NET_F_MAC) |
		(1<<VIRTIO_NET_F_STATUS) |
		(1<<VIRTIO_NET_F_CSUM);

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

	if (!init_rx(&dev)) {
		exit(ERR);
	}

	if (!init_tx(&dev)) {
		exit(ERR);
	}

	dev.ipv4[0] = 192;
	dev.ipv4[1] = 168;
	dev.ipv4[2] = 10;
	dev.ipv4[3] = 34;

	test(&dev);

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
			handle_message(from, m);	
		}

	}

	return ERR;
}

