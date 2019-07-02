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
arp_request(struct net_dev *net, uint8_t *ipv4)
{
	uint8_t dst[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	uint8_t *pkt, *bdy;

	if (!create_eth_pkt(net, dst,
				0x0806,
				28,
				&pkt, (uint8_t **) &bdy))
	{
		return;
	}

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
	memcpy(&bdy[24], ipv4, 4);

	send_eth_pkt(net, pkt, 28);
}

	static void 
handle_arp_request(struct net_dev *net, uint8_t *src_mac, uint8_t *src_ipv4)
{
	uint8_t *pkt, *bdy;

	log(LOG_WARNING, "asking about us!!!");

	if (!create_eth_pkt(net, src_mac,
				0x0806,
				28,
				&pkt, (uint8_t **) &bdy))
	{
		return;
	}

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

	send_eth_pkt(net, pkt, 28);
}

	static void
handle_arp_response(struct net_dev *net, uint8_t *bdy, size_t len)
{
	log(LOG_INFO, "responding to us!!!");

	uint8_t *src_ip, *src_mac;
	uint8_t *tgt_ip, *tgt_mac;

	src_mac = bdy + 8;
	src_ip = bdy + 14;
	tgt_mac = bdy + 18;
	tgt_ip = bdy + 24;

	log(LOG_INFO, "arp response from %i.%i.%i.%i / %x:%x:%x:%x:%x:%x",
			src_ip[0],
			src_ip[1],
			src_ip[2],
			src_ip[3],
			src_mac[0],
			src_mac[1],
			src_mac[2],
			src_mac[3],
			src_mac[4],
			src_mac[5]);

	log(LOG_INFO, "to                %i.%i.%i.%i / %x:%x:%x:%x:%x:%x",
			tgt_ip[0],
			tgt_ip[1],
			tgt_ip[2],
			tgt_ip[3],
			tgt_mac[0],
			tgt_mac[1],
			tgt_mac[2],
			tgt_mac[3],
			tgt_mac[4],
			tgt_mac[5]);


}

	void
handle_arp(struct net_dev *net, 
		struct eth_hdr *hdr, 
		uint8_t *bdy, size_t len)
{
	uint16_t htype, ptype;
	uint8_t hlen, plen;
	uint16_t oper;

	static uint8_t broadcast_mac[6] = {
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff
	};

	if (len < 8) {
		log(LOG_WARNING, "arp packet with bad len %i", len);
		return;
	}

	htype = bdy[0] << 8 | bdy[1];
	ptype = bdy[2] << 8 | bdy[3];
	hlen = bdy[4];
	plen = bdy[5];
	oper = bdy[6] << 8 | bdy[7];

	if (len < 8 + hlen * 2 + plen * 2) {
		log(LOG_WARNING, "arp packet with bad len %i expected %i", 
				len, 8 + hlen * 2 + plen * 2);
		return;
	}

	if (htype != 1 || ptype != 0x800) {
		log(LOG_WARNING, "arp packet with unsupported protocols %i 0x%x",
				htype, ptype);
		return;
	}

	if (memcmp(hdr->dst, broadcast_mac, 6)) {
		uint8_t *src_ipv4 = bdy + 14;
		uint8_t *tgt_ipv4 = bdy + 24;

		if (memcmp(tgt_ipv4, net->ipv4, 4)) {
			handle_arp_request(net, hdr->src, src_ipv4);
		}

	} else if (memcmp(hdr->dst, net->mac, 6)) {

		switch (oper) {
			case 1:
				break;

			case 2:
				handle_arp_response(net, bdy, len);
				break;

			default:
				log(LOG_WARNING, "arp packet with bad oper %i", oper);
				return;
		}
	}
}

