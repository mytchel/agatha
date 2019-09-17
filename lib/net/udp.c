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
#include "ip.h"

	static bool
send_udp_pkt(struct net_dev *net,
		struct binding *c,
		uint8_t mac_rem[6],
		uint8_t addr_rem[4],
		uint16_t port_rem,
		uint8_t *data,
		size_t data_len)
{
	struct binding_ip *ip = c->proto_arg;
	struct ipv4_hdr *ipv4_hdr;
	struct udp_hdr *udp_hdr;
	uint8_t *pkt, *pkt_data;
	size_t length;

	length = sizeof(struct udp_hdr) + data_len;

	if (!create_ipv4_pkt(net, 
				mac_rem, addr_rem,
				20, IP_UDP,
				length,
				&pkt,
				&ipv4_hdr, 
				(uint8_t **) &udp_hdr)) 
	{
		return false;
	}
	
	udp_hdr->port_src[0] = (ip->port_loc >> 8) & 0xff;
	udp_hdr->port_src[1] = (ip->port_loc >> 0) & 0xff;
	udp_hdr->port_dst[0] = (port_rem >> 8) & 0xff;
	udp_hdr->port_dst[1] = (port_rem >> 0) & 0xff;

	udp_hdr->length[0] = (length >> 8) & 0xff;
	udp_hdr->length[1] = (length >> 0) & 0xff;

	udp_hdr->csum[0] = 0;
	udp_hdr->csum[1] = 0;

	pkt_data = ((uint8_t *) udp_hdr) + sizeof(struct udp_hdr);
	
	memcpy(pkt_data, data, data_len);

	send_ipv4_pkt(net, pkt, ipv4_hdr, 
				20, 
				length);

	free(pkt);

	return true;
}

	void
handle_udp(struct net_dev *net, 
		struct ip_pkt *p)
{
	struct udp_hdr *udp_hdr;
	uint16_t port_src, port_dst;
	size_t length, csum, data_len;
	uint8_t *data;

	udp_hdr = (void *) p->data;

	port_src = udp_hdr->port_src[0] << 8 | udp_hdr->port_src[1];
	port_dst = udp_hdr->port_dst[0] << 8 | udp_hdr->port_dst[1];
	length = udp_hdr->length[0] << 8 | udp_hdr->length[1];
	csum = udp_hdr->csum[0] << 8 | udp_hdr->csum[1];

	log(LOG_INFO, "udp packet from port %i to port %i",
			(size_t) port_src, (size_t) port_dst);

	if (length > p->len) {
		log(LOG_INFO, "udp packet length %i larger than packet length %i",
				length, p->len);
		return;
	}

	data = p->data + sizeof(struct udp_hdr);
	data_len = length - sizeof(struct udp_hdr);

	log(LOG_INFO, "udp packet length %i csum 0x%x",
			data_len, csum);

	dump_hex_block(data, data_len);

	struct binding *c;

	c = find_binding_ip(net, NET_proto_udp, port_dst);
	if (c == nil) {
		log(LOG_INFO, "got unhandled packet");
		ip_pkt_free(p);
		return;
	}

	struct binding_ip *ip = c->proto_arg;

	struct ip_pkt **o;
	for (o = &ip->udp.recv_wait; *o != nil; o = &(*o)->next) {
		log(LOG_INFO, "after pkt %i", (*o)->id);
	}
		;

	p->next = nil;
	*o = p;
}
#if 0
static void
send_udp_finish(struct net_dev *net,
		void *arg,
		uint8_t *mac)
{
	struct binding *c = arg;
	struct binding_ip *ip = c->proto_arg;
	union net_rsp rp;

	char mac_str[32];
	print_mac(mac_str, mac);

	log(LOG_INFO, "got mac dst %s", mac_str);

	memcpy(ip->mac_rem, mac, 6);

	rp.write.type = NET_open_rsp;
	rp.write.ret = OK;
	rp.write.id = c->id;

	send(c->proc_id, &rp);
}
#endif

int
ip_bind_udp(struct net_dev *net,
		union net_req *rq, 
		struct binding *c)
{
	struct binding_ip *ip;

	log(LOG_INFO, "bind udp");

	ip = malloc(sizeof(struct binding_ip));
	if (ip == nil) {
		log(LOG_WARNING, "out of mem");
		return ERR;
	}

	c->proto_arg = ip;

	memcpy(ip->addr_loc, rq->bind.addr_loc, 4);
	ip->port_loc = rq->bind.port_loc;

	ip->udp.offset_into_waiting = 0;
	ip->udp.recv_wait = nil;

/*
	struct net_dev_internal *i = net->internal;
	memcpy(ip->ip_rem, rq->open.addr, 4);
	arp_request(net, ip->ip_rem, &open_udp_finish, c);
*/

	return OK;
}

int
ip_unbind_udp(struct net_dev *net,
		union net_req *rq, 
		struct binding *c)
{
	log(LOG_WARNING, "todo udp unbind");
	return OK;
}

void
ip_write_udp(struct net_dev *net,
		union net_req *rq, 
		struct binding *c)
{
	union net_rsp rp;

	log(LOG_INFO, "write udp");

	uint8_t *va;

	va = map_addr(rq->write.pa, rq->write.pa_len, MAP_RO);
	if (va == nil) {
		return;
	}

	uint8_t mac_rem[6] = { 0 };

	if (!send_udp_pkt(net, c, 
			mac_rem,
			rq->write.proto.udp.addr_rem, 
			rq->write.proto.udp.port_rem,
			va, rq->write.len))
	{
		log(LOG_INFO, "create pkt fail");
		return;
	}

	log(LOG_INFO, "sent");

	unmap_addr(va, rq->write.pa_len);

	if (give_addr(c->proc_id, rq->write.pa, rq->write.pa_len) != OK) {
		return;
	}

	rp.write.type = NET_write_rsp;
	rp.write.ret = OK;
	rp.write.chan_id = c->id;
	rp.write.pa = rq->write.pa;
	rp.write.pa_len = rq->write.pa_len;
	rp.write.len = rq->write.len;

	send(c->proc_id, &rp);
}

void
ip_read_udp(struct net_dev *net,
		union net_req *rq, 
		struct binding *c)
{
	struct binding_ip *ip = c->proto_arg;
	size_t len, l, o;
	struct ip_pkt *p;
	union net_rsp rp;
	uint8_t *va;

	log(LOG_INFO, "read udp");

	va = map_addr(rq->read.pa, rq->read.pa_len, MAP_RW);
	if (va == nil) {
		return;
	}

	len = 0;
	while (ip->udp.recv_wait != nil && len < rq->read.len) {
		p = ip->udp.recv_wait;

		log(LOG_INFO, "read from pkt 0x%x", p->id);

		o = ip->udp.offset_into_waiting;
		log(LOG_INFO, "o = %i", o);

		l = rq->read.len - len;
		log(LOG_INFO, "l = %i", l);
		if (l > p->len - sizeof(struct udp_hdr) - o) {
			l = p->len - sizeof(struct udp_hdr) - o;
			log(LOG_INFO, "l cut to = %i", l);
		}

		log(LOG_INFO, "read %i bytes from %i offset", l, o);
		memcpy(va + len, 
			p->data + sizeof(struct udp_hdr) + o,
		 	l);

		if (o + l < p->len - sizeof(struct udp_hdr)) {
			log(LOG_INFO, "update offset to %i", o + l);
			ip->udp.offset_into_waiting = o + l;
		} else {
			log(LOG_INFO, "shift to next pkt");
			ip->udp.offset_into_waiting = 0;
			ip->udp.recv_wait = p->next;
			ip_pkt_free(p);
		}
		
		len += l;
	}

	unmap_addr(va, rq->read.pa_len);

	log(LOG_INFO, "got %i bytes in total", len);

	if (give_addr(c->proc_id, rq->read.pa, rq->read.pa_len) != OK) {
		return;
	}

	rp.read.type = NET_read_rsp;
	rp.read.ret = OK;
	rp.read.chan_id = c->id;
	rp.read.pa = rq->read.pa;
	rp.read.pa_len = rq->read.pa_len;
	rp.read.len = len;

	send(c->proc_id, &rp);
}

