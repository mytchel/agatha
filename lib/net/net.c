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
#include <ip.h>
#include <net_dev.h>
#include "net.h"

	void
net_process_pkt(struct net_dev *net, uint8_t *pkt, size_t len)
{
	struct eth_hdr *hdr;
	uint8_t *bdy;
	size_t bdy_len;

	hdr	= (void *) pkt;

	bdy	= pkt + sizeof(struct eth_hdr);
	bdy_len = len - sizeof(struct eth_hdr);

	uint32_t type = hdr->tol.type[0] << 8 | hdr->tol.type[1];

	if (memcmp(hdr->dst, net->mac, 6)) {
		log(LOG_WARNING, "this packet is for us ::::");
		dump_hex_block(pkt, len);
	}

	switch (type) {
		case 0x0806:
			handle_arp(net, hdr,
					bdy, bdy_len);
			break;

		case 0x0800:
			handle_ipv4(net, hdr,
					bdy, bdy_len);
			break;

		default:
			break;
	}
}

	void
net_handle_message(struct net_dev *net,
		int from, uint8_t *m)
{
	uint32_t type = *((uint32_t *) m);

	switch (type) {
		default:
			break;
	}
}

	int
net_init(struct net_dev *net)
{
	char mac_str[18];

	print_mac(mac_str, net->mac);
	log(LOG_INFO, "mac = %s", mac_str);

	net->ipv4[0] = 192;
	net->ipv4[1] = 168;
	net->ipv4[2] = 10;
	net->ipv4[3] = 34;

	net->ipv4_ident = 0xabcd;

	return OK;
}

