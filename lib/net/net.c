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
#include <net.h>
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

static struct connection *
find_connection(struct net_dev *net,
		int pid, int id)
{
	struct net_dev_internal *i = net->internal;
	struct connection *c;

	for (c = i->connections; c != nil; c = c->next) {
		if (c->proc_id == pid && c->id == id) {
			return c;
		}
	}	

	return nil;
}


static void
handle_open(struct net_dev *net,
		int from, union net_req *rq)
{
	struct net_dev_internal *i = net->internal;
	struct connection *c;
	union net_rsp rp;

	log(LOG_INFO, "%i want to open conneciton", from);

	if ((c = malloc(sizeof(struct connection))) == nil) {
		rp.open.type = NET_open_req;
		rp.open.ret = ERR;
		send(from, &rp);
		return;
	}

	c->proc_id = from;
	c->id = i->n_connection_id++;

	c->proto = rq->open.proto;

	c->next = i->connections;
	i->connections = c;

	switch (rq->open.proto) {
		case NET_UDP:
			ip_open_udp(net, rq, c);
			break;

		case NET_TCP:
			ip_open_tcp(net, rq, c);
			break;

		default:
			rp.open.type = NET_open_req;
			rp.open.ret = ERR;
			send(from, &rp);
			return;
	}
}

	static void
handle_close(struct net_dev *net,
		int from, union net_req *rq)
{
	struct connection *c;
	union net_rsp rp;
	int ret;

	log(LOG_INFO, "%i want to close", from);

	c = find_connection(net, from, rq->read.id);
	if (c == nil) {
		log(LOG_INFO, "connection %i for %i not found",
			rq->read.id, from);
		return;
	}

	switch (c->proto) {
		case NET_UDP:
			ret = ip_close_udp(net, from, rq, c);
			break;

		case NET_TCP:
			ret = ip_close_tcp(net, from, rq, c);
			break;

		default:
			ret = ERR;
			break;
	}

	rp.open.type = NET_close_req;
	rp.open.ret = ret;
	rp.open.id = rq->read.id;
	send(from, &rp);
}

	static void
handle_read(struct net_dev *net,
		int from, union net_req *rq)
{
	struct connection *c;

	log(LOG_INFO, "%i want to read", from);

	c = find_connection(net, from, rq->read.id);
	if (c == nil) {
		log(LOG_INFO, "connection %i for %i not found",
			rq->read.id, from);
		return;
	}

	log(LOG_INFO, "found con has proto %i", c->proto);

	switch (c->proto) {
		case NET_UDP:
			ip_read_udp(net, from, rq, c);
			break;

		case NET_TCP:
			ip_read_tcp(net, from, rq, c);
			break;

		default:
			return;
	}
}

	static void
handle_write(struct net_dev *net,
		int from, union net_req *rq)
{
	struct connection *c;

	log(LOG_INFO, "%i want to write", from);

	c = find_connection(net, from, rq->write.id);
	if (c == nil) {
		log(LOG_INFO, "connection %i for %i not found",
			rq->write.id, from);
		return;
	}

	switch (c->proto) {
		case NET_UDP:
			ip_write_udp(net, from, rq, c);
			break;

		case NET_TCP:
			ip_write_tcp(net, from, rq, c);
			break;

		default:
			return;
	}
}

	void
net_handle_message(struct net_dev *net,
		int from, uint8_t *m)
{
	uint32_t type = *((uint32_t *) m);

	switch (type) {
		case NET_open_req:
			handle_open(net, from, (void *) m);
			break;

		case NET_close_req:
			handle_close(net, from, (void *) m);
			break;

		case NET_read_req:
			handle_read(net, from, (void *) m);
			break;

		case NET_write_req:
			handle_write(net, from, (void *) m);
			break;

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

	i->arp_entries = nil;
	i->arp_requests = nil;

	i->n_free_port = 1025;
	i->n_connection_id = 321;
	i->connections = nil;

	net->internal = i;

	print_mac(mac_str, net->mac);
	log(LOG_INFO, "mac = %s", mac_str);

	net->ipv4[0] = 192;
	net->ipv4[1] = 168;
	net->ipv4[2] = 10;
	net->ipv4[3] = 35;

	net->ipv4_ident = 0xabcd;

	uint8_t ip[4] = { 192, 168, 1, 1};
	arp_request(net, ip, &f, nil);

	union dev_reg_req drq;
	union dev_reg_rsp drp;

	drq.reg.type = DEV_REG_register_req;
	drq.reg.pid = pid();
	strlcpy(drq.reg.name, net->name, sizeof(drq.reg.name));

	log(LOG_INFO, "register as %s", net->name);

	if (mesg(DEV_REG_PID, &drq, &drp) != OK) {
		return ERR;
	} 
	
	log(LOG_INFO, "registered as %s", net->name);

	return drp.reg.ret;
}

