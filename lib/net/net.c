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

	static void
f(struct net_dev *net, void *arg, uint8_t *mac)
{
	log(LOG_INFO, "test arp request responded with %x:%x:%x:%x:%x:%x",
			mac[0],
			mac[1],
			mac[2],
			mac[3],
			mac[4],
			mac[5]);
}

	int
net_init(struct net_dev *net)
{
	struct net_dev_internal *i;
	char mac_str[18];

	if ((i = malloc(sizeof(struct net_dev_internal))) == nil) {
		return ERR;
	}

	i->ip_ports = nil;
	i->arp_entries = nil;
	i->arp_requests = nil;

	net->internal = i;

	print_mac(mac_str, net->mac);
	log(LOG_INFO, "mac = %s", mac_str);

	net->ipv4[0] = 192;
	net->ipv4[1] = 168;
	net->ipv4[2] = 10;
	net->ipv4[3] = 34;

	net->ipv4_ident = 0xabcd;

	struct ip_port *p;

	if ((p = malloc(sizeof(struct ip_port))) == nil) 
		return ERR;

	p->handler_pid = -1;
	p->port = 42;
	p->waiting_pkts = nil;
	p->next = nil;
	i->ip_ports = p;

	uint8_t ip[4] = { 192, 168, 1, 1};
	arp_request(net, ip, &f, nil);

	return OK;
}

