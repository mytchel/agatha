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
	
void
handle_arp(struct net_dev *net, 
		struct eth_hdr *hdr, 
		uint8_t *bdy, size_t len)
{
	static uint8_t broadcast_mac[6] = {
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff
	};

	if (memcmp(hdr->dst, broadcast_mac, 6)) {
		uint8_t *src_ipv4 = bdy + 14;
		uint8_t *dst_ipv4 = bdy + 24;

		if (memcmp(dst_ipv4, net->ipv4, 4)) {
			log(LOG_WARNING, "asking about us!!!");

			handle_arp_request(net, hdr->src, src_ipv4);
		}

	} else if (memcmp(hdr->dst, net->mac, 6)) {
		log(LOG_INFO, "responding to us!!!");
	}
}

