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
		struct connection *c,
		uint8_t *data,
		size_t data_len)
{
	struct connection_ip *ip = c->proto_arg;
	struct ipv4_hdr *ipv4_hdr;
	struct udp_hdr *udp_hdr;
	uint8_t *pkt, *pkt_data;
	size_t length;

	length = sizeof(struct udp_hdr) + data_len;

	if (!create_ipv4_pkt(net, 
				ip->mac_rem, ip->ip_rem,
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
	udp_hdr->port_dst[0] = (ip->port_rem >> 8) & 0xff;
	udp_hdr->port_dst[1] = (ip->port_rem >> 0) & 0xff;

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

	struct connection *c;

	c = find_connection_ip(net, NET_UDP, 
			port_dst, port_src, 
			p->src_ipv4);

	if (c == nil) {
		log(LOG_INFO, "got unhandled packet");
		ip_pkt_free(p);
		return;
	}

	struct connection_ip *ip = c->proto_arg;

	struct ip_pkt **o;
	for (o = &ip->recv_wait; *o != nil; o = &(*o)->next) {
		log(LOG_INFO, "after pkt %i", (*o)->id);
	}
		;

	p->next = nil;
	*o = p;
}

static void
open_udp_finish(struct net_dev *net,
		void *arg,
		uint8_t *mac)
{
	struct connection *c = arg;
	struct connection_ip *ip = c->proto_arg;
	union net_rsp rp;

	char mac_str[32];
	print_mac(mac_str, mac);

	log(LOG_INFO, "got mac dst %s", mac_str);

	memcpy(ip->mac_rem, mac, 6);

	rp.open.type = NET_open_rsp;
	rp.open.ret = OK;
	rp.open.id = c->id;

	send(c->proc_id, &rp);
}

void
ip_open_udp(struct net_dev *net,
		union net_req *rq, 
		struct connection *c)
{
	struct net_dev_internal *i = net->internal;
	struct connection_ip *ip;

	log(LOG_INFO, "open udp");

	ip = malloc(sizeof(struct connection_ip));
	if (ip == nil) {
		log(LOG_WARNING, "out of mem");
		return;
	}

	c->proto_arg = ip;

	ip->port_loc = i->n_free_port++;;
	ip->port_rem = rq->open.port;

	memcpy(ip->ip_rem, rq->open.addr, 4);

	ip->offset_into_waiting = 0;
	ip->recv_wait = nil;

	arp_request(net, ip->ip_rem, &open_udp_finish, c);
}

void
ip_write_udp(struct net_dev *net,
		int from,
		union net_req *rq, 
		struct connection *c)
{
	union net_rsp rp;

	log(LOG_INFO, "write udp");

	uint8_t *va;

	va = map_addr(rq->write.pa, rq->write.len, MAP_RO);
	if (va == nil) {
		return;
	}

	if (!send_udp_pkt(net,
			c, va,
			rq->write.w_len))
	{
		log(LOG_INFO, "create pkt fail");
		return;
	}

	log(LOG_INFO, "sent");

	unmap_addr(va, rq->write.len);

	if (give_addr(from, rq->write.pa, rq->write.len) != OK) {
		return;
	}

	rp.write.type = NET_write_rsp;
	rp.write.ret = OK;
	rp.write.id = c->id;
	rp.write.pa = rq->write.pa;
	rp.write.len = rq->write.len;
	rp.write.w_len = rq->write.w_len;

	send(from, &rp);
}

void
ip_read_udp(struct net_dev *net,
		int from,
		union net_req *rq, 
		struct connection *c)
{
	struct connection_ip *ip = c->proto_arg;
	size_t r_len, l, o;
	struct ip_pkt *p;
	union net_rsp rp;
	uint8_t *va;

	log(LOG_INFO, "read udp");

	va = map_addr(rq->read.pa, rq->read.len, MAP_RW);
	if (va == nil) {
		return;
	}

	r_len = 0;
	while (ip->recv_wait != nil && r_len < rq->read.r_len) {
		p = ip->recv_wait;

		log(LOG_INFO, "read from pkt 0x%x", p->id);

		o = ip->offset_into_waiting;
		log(LOG_INFO, "o = %i", o);

		l = rq->read.r_len - r_len;
		log(LOG_INFO, "l = %i", l);
		if (l > p->len - o) {
			l = p->len - o;
			log(LOG_INFO, "l cut to = %i", l);
		}

		log(LOG_INFO, "read %i bytes from %i offset", l, o);
		memcpy(va + r_len, 
			p->data + o,
		 	l);

		if (o + l < p->len) {
			log(LOG_INFO, "update offset to %i", o + l);
			ip->offset_into_waiting = o + l;
		} else {
			log(LOG_INFO, "shift to next pkt");
			ip->offset_into_waiting = 0;
			ip->recv_wait = p->next;
			ip_pkt_free(p);
		}
		
		r_len += l;
	}

	unmap_addr(va, rq->read.len);

	log(LOG_INFO, "got %i bytes in total", r_len);

	if (give_addr(from, rq->read.pa, rq->read.len) != OK) {
		return;
	}

	rp.read.type = NET_read_rsp;
	rp.read.ret = OK;
	rp.read.id = c->id;
	rp.read.pa = rq->read.pa;
	rp.read.len = rq->read.len;
	rp.read.r_len = r_len;

	send(from, &rp);
}

int
ip_close_udp(struct net_dev *net,
		int from,
		union net_req *rq, 
		struct connection *c)
{
	log(LOG_WARNING, "todo udp close");
	return OK;
}


