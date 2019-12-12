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

static struct binding *
find_binding(struct net_dev *net,
		int pid, int id)
{
	struct net_dev_internal *i = net->internal;
	struct binding *c;

	for (c = i->bindings; c != nil; c = c->next) {
		if (c->proc_id == pid && c->id == id) {
			return c;
		}
	}	

	return nil;
}


static void
handle_bind(struct net_dev *net,
		int eid, int from, union net_req *rq)
{
	struct net_dev_internal *i = net->internal;
	struct binding *b;
	union net_rsp rp;
	int ret;

	log(LOG_INFO, "%i wants to bind", from);

	if ((b = malloc(sizeof(struct binding))) == nil) {
		rp.bind.type = NET_bind;
		rp.bind.ret = ERR;
		reply(eid, from, &rp);
		return;
	}

	b->proc_id = from;
	b->id = i->n_binding_id++;

	b->proto = rq->bind.proto;

	ret = ip_bind(net, rq, b);

	log(LOG_INFO, "ret = %i", ret);

	if (ret == OK) {
		b->next = i->bindings;
		i->bindings = b;
	} else {
		free(b);
	}

	rp.bind.type = NET_bind;
	rp.bind.ret = ret;
	rp.bind.chan_id = b->id;
	reply(eid, from, &rp);
}

	static void
handle_unbind(struct net_dev *net,
		int eid, int from, union net_req *rq)
{
	struct net_dev_internal *i = net->internal;
	struct binding *b, **o;
	union net_rsp rp;
	int ret;

	log(LOG_INFO, "%i want to unbind", from);

	b = find_binding(net, from, rq->unbind.chan_id);
	if (b == nil) {
		log(LOG_INFO, "connection %i for %i not found",
			rq->unbind.chan_id, from);
		return;
	}

	if (b->proc_id != from) {
		return;
	}

	ret = ip_unbind(net, rq, b);

	for (o = &i->bindings; *o != nil; o = &(*o)->next) {
		if (*o == b) break;
	}

	*o = b->next;

	free(b);

	rp.unbind.type = NET_unbind;
	rp.unbind.ret = ret;
	rp.unbind.chan_id = rq->unbind.chan_id;
	reply(eid, from, &rp);
}

	static void
handle_tcp_listen(struct net_dev *net,
		int eid, int from, union net_req *rq)
{
	struct binding *c;

	log(LOG_INFO, "%i want to listen", from);

	c = find_binding(net, from, rq->tcp_listen.chan_id);
	if (c == nil) {
		log(LOG_INFO, "connection %i for %i not found",
			rq->tcp_listen.chan_id, from);
		return;
	}

	if (c->proc_id != from) {
		return;
	}

	switch (c->proto) {
	default:
		return;

	case NET_proto_tcp:
		ip_tcp_listen(net, rq, c);
		break;
	}
}

	static void
handle_tcp_connect(struct net_dev *net,
		int eid, int from, union net_req *rq)
{
	struct binding *c;

	log(LOG_INFO, "%i want to connect", from);

	c = find_binding(net, from, rq->tcp_connect.chan_id);
	if (c == nil) {
		log(LOG_INFO, "connection %i for %i not found",
			rq->tcp_connect.chan_id, from);
		return;
	}

	if (c->proc_id != from) {
		return;
	}

	switch (c->proto) {
	default:
		return;

	case NET_proto_tcp:
		ip_tcp_connect(net, rq, c);
		break;
	}
}

	static void
handle_tcp_disconnect(struct net_dev *net,
		int eid, int from, union net_req *rq)
{
	struct binding *c;

	log(LOG_INFO, "%i want to disconnect", from);

	c = find_binding(net, from, rq->tcp_disconnect.chan_id);
	if (c == nil) {
		log(LOG_INFO, "connection %i for %i not found",
			rq->tcp_disconnect.chan_id, from);
		return;
	}

	if (c->proc_id != from) {
		return;
	}

	switch (c->proto) {
	default:
		return;

	case NET_proto_tcp:
		ip_tcp_disconnect(net, rq, c);
		break;
	}
}

	static void
handle_read(struct net_dev *net,
		int eid, int from, union net_req *rq, int cap)
{
	struct binding *c;

	log(LOG_INFO, "%i want to read", from);

	c = find_binding(net, from, rq->read.chan_id);
	if (c == nil) {
		log(LOG_INFO, "connection %i for %i not found",
			rq->read.chan_id, from);
		return;
	}

	if (c->proc_id != from) {
		return;
	}

	switch (c->proto) {
	case NET_proto_udp:
		ip_read_udp(net, rq, cap, c);
		break;

	case NET_proto_tcp:
		ip_read_tcp(net, rq, cap, c);
		break;

	default:
		return;
	}
}

	static void
handle_write(struct net_dev *net,
		int eid, int from, union net_req *rq, int cap)
{
	struct binding *c;

	log(LOG_INFO, "%i want to write", from);

	c = find_binding(net, from, rq->write.chan_id);
	if (c == nil) {
		log(LOG_INFO, "connection %i for %i not found",
			rq->write.chan_id, from);
		return;
	}

	if (c->proc_id != from) {
		return;
	}

	switch (c->proto) {
	case NET_proto_udp:
		ip_write_udp(net, rq, cap, c);
		break;

	case NET_proto_tcp:
		ip_write_tcp(net, rq, cap, c);
		break;

	default:
		return;
	}
}

	void
net_handle_message(struct net_dev *net,
		int eid, int from, uint8_t *m, int cap)
{
	uint32_t type = *((uint32_t *) m);

	switch (type) {
		case NET_bind:
			handle_bind(net, eid, from, (void *) m);
			break;

		case NET_unbind:
			handle_unbind(net, eid, from, (void *) m);
			break;

		case NET_tcp_listen:
			handle_tcp_listen(net, eid, from, (void *) m);
			break;

		case NET_tcp_connect:
			handle_tcp_connect(net, eid, from, (void *) m);
			break;

		case NET_tcp_disconnect:
			handle_tcp_disconnect(net, eid, from, (void *) m);
			break;

		case NET_read:
			handle_read(net, eid, from, (void *) m, cap);
			break;

		case NET_write:
			handle_write(net, eid, from, (void *) m, cap);
			break;

		default:
			break;
	}
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
	i->n_binding_id = 321;
	i->bindings = nil;

	net->internal = i;

	print_mac(mac_str, net->mac);
	log(LOG_INFO, "mac = %s", mac_str);

	net->ipv4[0] = 192;
	net->ipv4[1] = 168;
	net->ipv4[2] = 10;
	net->ipv4[3] = 34;

	net->ipv4_ident = 0xabcd;

	return OK;
}

