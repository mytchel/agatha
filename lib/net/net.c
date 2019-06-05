#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <proc0.h>
#include <stdarg.h>
#include <string.h>
#include <dev_reg.h>
#include <log.h>
#include <eth.h>
#include <net.h>

const uint8_t broadcast_mac[6] = { 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff 
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

	static void 
test_respond_arp(struct net_dev *net, uint8_t *src_mac, uint8_t *src_ipv4)
{
	struct eth_hdr *hdr;
	uint8_t *pkt, *bdy;

	pkt = malloc(sizeof(struct eth_hdr) + 64);
	if (pkt == nil) {
		log(LOG_WARNING, "malloc failed for pkt");
		return;
	}

	log(LOG_INFO, "building arp response packet");

	hdr = (void *) pkt;
	bdy = pkt + sizeof(struct eth_hdr);

	memcpy(hdr->dst, src_mac, 6);
	memcpy(hdr->src, net->mac, 6);
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

	memcpy(&bdy[8], net->mac, 6);
	memcpy(&bdy[14], net->ipv4, 4);
	memcpy(&bdy[18], src_mac, 6);
	memcpy(&bdy[24], src_ipv4, 4);

	memset(&bdy[28], 0, 64 - sizeof(struct eth_hdr) - 28);

	net->send_pkt(net, pkt, 64);
}

	static void
test(struct net_dev *net)
{
	struct eth_hdr *hdr;
	uint8_t *bdy, *pkt;

	pkt = malloc(sizeof(struct eth_hdr) + 64);
	if (pkt == nil) {
		log(LOG_WARNING, "malloc failed for pkt");
		return;
	}

	log(LOG_INFO, "building test packet");

	hdr = (void *) pkt;
	bdy = pkt + sizeof(struct eth_hdr);

	uint8_t dst[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	uint8_t dst_ipv4[4] = { 192, 168, 10, 1 };

	memcpy(hdr->dst, dst, 6);
	memcpy(hdr->src, net->mac, 6);
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

	memcpy(&bdy[8], net->mac, 6);
	memcpy(&bdy[14], net->ipv4, 4);
	memset(&bdy[18], 0, 6);
	memcpy(&bdy[24], dst_ipv4, 4);

	memset(&bdy[28], 0, 64 - sizeof(struct eth_hdr) - 28);

	net->send_pkt(net, pkt, 64);
}
	
static void
handle_arp(struct net_dev *net, 
		struct eth_hdr *hdr, 
		uint8_t *body, size_t len)
{
	log(LOG_INFO, "have arp packet!");

	if (memcmp(hdr->dst, broadcast_mac, 6)) {
		log(LOG_INFO, "broadcast arp");

		uint8_t *src_ipv4 = body + 14;
		uint8_t *dst_ipv4 = body + 24;

		log(LOG_INFO, "from    for %i.%i.%i.%i",
				src_ipv4[0], src_ipv4[1], src_ipv4[2], src_ipv4[3]);

		log(LOG_INFO, "looking for %i.%i.%i.%i",
				dst_ipv4[0], dst_ipv4[1], dst_ipv4[2], dst_ipv4[3]);

		if (memcmp(dst_ipv4, net->ipv4, 4)) {
			log(LOG_WARNING, "asking about us!!!");

			test_respond_arp(net, hdr->src, src_ipv4);
		}

	} else if (memcmp(hdr->dst, net->mac, 6)) {
		log(LOG_INFO, "responding to us!!!");
	}
}

	void
net_process_pkt(struct net_dev *net, uint8_t *pkt, size_t len)
{
	struct eth_hdr *hdr;
	uint8_t *bdy;
	size_t bdy_len;
 
	hdr	= (void *) pkt;

	bdy	= pkt + sizeof(struct eth_hdr);
	bdy_len = len - sizeof(struct eth_hdr);

	log(LOG_INFO, "process pkt len %i", len);

	uint32_t type = hdr->tol.type[0] << 8 | hdr->tol.type[1];

	char dst[18], src[18];

	print_mac(dst, hdr->dst);
	print_mac(src, hdr->src);

	log(LOG_INFO, "from %s", src);
	log(LOG_INFO, "to   %s", dst);
	log(LOG_INFO, "type 0x%x", type);

	switch (type) {
		case 0x0806:
			handle_arp(net, hdr,
					bdy, bdy_len);
			break;

		default:
			break;
	}

	if (memcmp(hdr->dst, net->mac, 6)) {
		log(LOG_WARNING, "this packet is for us ::::");
		dump_hex_block(pkt, len);
	}
}

	void
net_handle_message(struct net_dev *dev,
		int from, uint8_t *m)
{
	uint32_t type = *((uint32_t *) m);

	switch (type) {
		default:
			break;
	}
}

int
net_init(struct net_dev *dev)
{
	char mac_str[18];

	print_mac(mac_str, dev->mac);
	log(LOG_INFO, "mac = %s", mac_str);

	dev->ipv4[0] = 192;
	dev->ipv4[1] = 168;
	dev->ipv4[2] = 10;
	dev->ipv4[3] = 34;

	test(dev);

	return OK;
}

