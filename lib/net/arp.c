#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <proc0.h>
#include <stdarg.h>
#include <string.h>
#include <log.h>
#include <eth.h>
#include <ip.h>
#include <net_dev.h>
#include <net.h>
#include "net.h"

bool
arp_match_ip(struct net_dev *net, 
		uint8_t *ip_src, 
		uint8_t *mac_dst)
{
	struct net_dev_internal *i = net->internal;
	struct arp_entry *e;

	for (e = i->arp_entries; e != nil; e = e->next)	{
		if (memcmp(e->ip, ip_src, 4)) {
			memcpy(mac_dst, e->mac, 6);
			return true;
		}
	}

	return false;
}

	void
arp_request(struct net_dev *net, uint8_t *ipv4, 
		void (*func)(struct net_dev *net, void *arg, uint8_t *mac),
		void *arg)
{
	uint8_t dst[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	uint8_t mac[6];

	if (arp_match_ip(net, ipv4, mac)) {
		func(net, arg, mac);
		return;
	}

	struct net_dev_internal *i = net->internal;
	struct arp_request *r;

	uint8_t *pkt, *bdy;

	if (!create_eth_pkt(net, dst,
				0x0806,
				28,
				&pkt, (uint8_t **) &bdy))
	{
		return;
	}

	if ((r = malloc(sizeof(struct arp_request))) == nil) {
		free(pkt);
		return;
	}

	memcpy(r->ip, ipv4, 4);
	r->func = func;
	r->arg = arg;
	r->next = i->arp_requests;
	i->arp_requests = r;

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
arp_add_entry(struct net_dev *net, uint8_t *ipv4, uint8_t *mac)
{
	struct net_dev_internal *i = net->internal;
	struct arp_request **r, *f;
	struct arp_entry *e;

	log(LOG_INFO, "adding entry for %i.%i.%i.%i", ipv4[0], ipv4[1], ipv4[2], ipv4[3]);

	for (e = i->arp_entries; e != nil; e = e->next) {
		if (memcmp(e->ip, ipv4, 4)) {
			break;
		}
	}

	if (e == nil) {
		if ((e = malloc(sizeof(struct arp_entry))) == nil) {
			log(LOG_WARNING, "malloc failed");
			return;
		}

		e->next = i->arp_entries;
		i->arp_entries = e;
		memcpy(e->ip, ipv4, 4);
	}

	memcpy(e->mac, mac, 6);

	r = &i->arp_requests; 
	while (*r != nil) {
		if (memcmp((*r)->ip, ipv4, 4)) {
			f = *r;
			*r = (*r)->next;
			f->func(net, f->arg, mac);
			free(f);
	
		} else {
			r = &(*r)->next;
		}
	}
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

	arp_add_entry(net, src_ipv4, src_mac);
}

	static void
handle_arp_response(struct net_dev *net, uint8_t *bdy, size_t len)
{
	uint8_t *src_ip, *src_mac;

	log(LOG_INFO, "got arp response");

	src_mac = bdy + 8;
	src_ip = bdy + 14;

	arp_add_entry(net, src_ip, src_mac);
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

