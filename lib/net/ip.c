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

static void
ip_pkt_free(struct ip_pkt *p);

	static int16_t 
csum_ip(uint8_t *h, size_t len)
{
	size_t sum, s, i;
	uint16_t v;

	sum = 0;
	for (i = 0; i < len; i += 2) {
		v = (h[i] << 8) | h[i+1];
		sum += v;
	}

	while (sum > 0xffff) {
		s = sum >> 16;
		sum = (sum & 0xffff) + s;
	}
	
	return (~sum);
}

static bool
create_ipv4_pkt(struct net_dev *net,
		uint8_t *dst_mac, uint8_t *dst_ip,
		size_t hdr_len,
		uint8_t proto,
		size_t data_len,
		uint8_t **pkt,
		struct ipv4_hdr **ipv4_hdr,
		uint8_t **body)
{
	size_t ip_len, frag_flags, frag_offset, csum;
	struct ipv4_hdr *ip_hdr;

	ip_len = hdr_len + data_len;

	if (!create_eth_pkt(net, dst_mac,
				0x0800,
				ip_len,
				pkt, (uint8_t **) &ip_hdr))
	{
		return false;
	}

	ip_hdr->ver_len = (4 << 4) | (hdr_len >> 2);

	ip_hdr->length[0] = (ip_len >> 8) & 0xff;
	ip_hdr->length[1] = (ip_len >> 0) & 0xff;

	ip_hdr->ident[0] = (net->ipv4_ident >> 8) & 0xff;
	ip_hdr->ident[1] = (net->ipv4_ident >> 0) & 0xff;

	net->ipv4_ident++;

	frag_flags = 0;
	frag_offset = 0;

	ip_hdr->fragment[0] = (frag_flags << 5) | 
		((frag_offset >> 8) & 0x1f);
	ip_hdr->fragment[1] = 
		(frag_offset & 0xff);

	ip_hdr->ttl = 0xff;
	ip_hdr->protocol = proto;

	memcpy(ip_hdr->dst, dst_ip, 4);
	memcpy(ip_hdr->src, net->ipv4, 4);

	ip_hdr->hdr_csum[0] = 0;
	ip_hdr->hdr_csum[1] = 0;

	csum = csum_ip((uint8_t *) ip_hdr, hdr_len);

	ip_hdr->hdr_csum[0] = (csum >> 8) & 0xff;
	ip_hdr->hdr_csum[1] = (csum >> 0) & 0xff;

	*ipv4_hdr = ip_hdr;
	*body = ((uint8_t *) ip_hdr) + hdr_len;

	return true;
}

	static void
send_ipv4_pkt(struct net_dev *net,
		uint8_t *pkt,
		struct ipv4_hdr *ip_hdr, 
		size_t hdr_len,
		size_t data_len)
{
	send_eth_pkt(net, pkt, hdr_len + data_len);
}

	static bool
create_icmp_pkt(struct net_dev *net,
		uint8_t *dst_mac, uint8_t *dst_ip,
		size_t data_len,
		uint8_t **pkt,
		struct ipv4_hdr **ipv4_hdr,
		struct icmp_hdr **icmp_hdr,
		uint8_t **data)
{
	if (!create_ipv4_pkt(net, 
				dst_mac, dst_ip,
				20, IP_ICMP,
				sizeof(struct icmp_hdr) + data_len,
				pkt,
				ipv4_hdr, 
				(uint8_t **) icmp_hdr)) 
	{
		return false;
	}

	*data = ((uint8_t *) *icmp_hdr) + sizeof(struct icmp_hdr);
	return true;
}

	static void
send_icmp_pkt(struct net_dev *net,
		uint8_t *pkt,
		struct ipv4_hdr *ip_hdr, 
		struct icmp_hdr *icmp_hdr,
		size_t data_len)
{
	size_t csum;

	icmp_hdr->csum[0] = 0;
	icmp_hdr->csum[1] = 0;

	log(LOG_INFO, "csum");
	csum = csum_ip((uint8_t *) icmp_hdr, 
			sizeof(struct icmp_hdr) + data_len);
	log(LOG_INFO, "csum = 0x%x", csum);

	icmp_hdr->csum[0] = (csum >> 8) & 0xff;
	icmp_hdr->csum[1] = (csum >> 0) & 0xff;

	log(LOG_INFO, "send");
	send_ipv4_pkt(net, pkt, ip_hdr, 
				20, 
				sizeof(struct icmp_hdr) +	data_len);
	log(LOG_INFO, "sent");
}

	static bool
create_udp_pkt(struct net_dev *net,
		uint8_t *dst_mac, uint8_t *dst_ip,
		uint16_t port_src, uint16_t port_dst,
		size_t data_len,
		uint8_t **pkt,
		struct ipv4_hdr **ipv4_hdr,
		struct udp_hdr **udp_hdr,
		uint8_t **data)
{
	size_t length;

	length = sizeof(struct udp_hdr) + data_len;

	if (!create_ipv4_pkt(net, 
				dst_mac, dst_ip,
				20, IP_UDP,
				length,
				pkt,
				ipv4_hdr, 
				(uint8_t **) udp_hdr)) 
	{
		return false;
	}

	(*udp_hdr)->port_src[0] = (port_src >> 8) & 0xff;
	(*udp_hdr)->port_src[1] = (port_src >> 0) & 0xff;
	(*udp_hdr)->port_dst[0] = (port_dst >> 8) & 0xff;
	(*udp_hdr)->port_dst[1] = (port_dst >> 0) & 0xff;

	(*udp_hdr)->length[0] = (length >> 8) & 0xff;
	(*udp_hdr)->length[1] = (length >> 0) & 0xff;

	(*udp_hdr)->csum[0] = 0;
	(*udp_hdr)->csum[1] = 0;

	*data = ((uint8_t *) *udp_hdr) + sizeof(struct udp_hdr);

	return true;
}

	static void
send_udp_pkt(struct net_dev *net,
		uint8_t *pkt,
		struct ipv4_hdr *ip_hdr, 
		struct udp_hdr *udp_hdr,
		size_t data_len)
{
	send_ipv4_pkt(net, pkt, ip_hdr, 
				20, 
				sizeof(struct udp_hdr) + data_len);
}

	static void 
handle_echo_request(struct net_dev *net,
		uint8_t *src_ipv4, 
		struct icmp_hdr *icmp_hdr,
		uint8_t *bdy,
		size_t bdy_len)
{
	struct ipv4_hdr *ip_hdr_r;
	struct icmp_hdr *icmp_hdr_r;
	uint8_t *pkt, *bdy_r;
	uint8_t rsp_mac[6];

	/* icmp */

	if (!arp_match_ip(net, src_ipv4, rsp_mac)) {
		log(LOG_WARNING, "got echo but don't have mac?");
		return;
	}

	log(LOG_INFO, "send echo reply");

	if (!create_icmp_pkt(net, rsp_mac, src_ipv4,
				bdy_len,
				&pkt, &ip_hdr_r, &icmp_hdr_r, &bdy_r)) {
		return;
	}

	icmp_hdr_r->type = 0;
	icmp_hdr_r->code = 0;

	memcpy(icmp_hdr_r->rst, icmp_hdr->rst, 4);

	memcpy(bdy_r, bdy, bdy_len);

	log(LOG_INFO, "send");
	send_icmp_pkt(net, pkt, ip_hdr_r, icmp_hdr_r, bdy_len);

	log(LOG_INFO, "free packet");
	free(pkt);
	log(LOG_INFO, "freed packet");
}

	static void
handle_icmp(struct net_dev *net, 
		struct ip_pkt *p)
{
	struct icmp_hdr *icmp_hdr;

	icmp_hdr = (void *) p->data;

	switch (icmp_hdr->type) {
		case 0:
			/* echo reply */
			break;

		case 8:
			/* echo request */
			handle_echo_request(net,
					p->src_ipv4,
					icmp_hdr,
					p->data + sizeof(struct icmp_hdr),
					p->len - sizeof(struct icmp_hdr));
			break;

		default:
			log(LOG_INFO, "other icmp type %i", icmp_hdr->type);
			break;
	}

	ip_pkt_free(p);
}

static struct connection *
find_connection(struct net_dev *net, 
		int proto,
		uint16_t port_loc, uint16_t port_rem,
		uint8_t ip_rem[4])
{
	struct net_dev_internal *i = net->internal;
	struct connection *c;

	for (c = i->connections; c != nil; c = c->next) {
		if (proto == c->proto
			&& memcmp(ip_rem, c->ip.ip_rem, 4)
			&& port_loc == c->ip.port_loc
			&& port_rem == c->ip.port_rem)
		{
			return c;
		}
	}

	return nil;
}


	static void
handle_tcp(struct net_dev *net, 
		struct ip_pkt *p)
{
	struct tcp_hdr *tcp_hdr;
	uint16_t port_src, port_dst;
	uint32_t seq, ack;
	size_t csum;

	tcp_hdr = (void *) p->data;

	port_src = tcp_hdr->port_src[0] << 8 | tcp_hdr->port_src[1];
	port_dst = tcp_hdr->port_dst[0] << 8 | tcp_hdr->port_dst[1];
	csum = tcp_hdr->csum[0] << 8 | tcp_hdr->csum[1];
	
	seq = tcp_hdr->seq[0] << 24 | tcp_hdr->seq[1] << 16 
		| tcp_hdr->seq[2] << 8 | tcp_hdr->seq[3];

	ack = tcp_hdr->ack[0] << 24 | tcp_hdr->ack[1] << 16 
		| tcp_hdr->ack[2] << 8 | tcp_hdr->ack[3];

	log(LOG_INFO, "tdp packet from port %i to port %i",
			(size_t) port_src, (size_t) port_dst);
	log(LOG_INFO, "tdp packet csum 0x%x seq %x ack 0x%x",
			(size_t) csum, (size_t) seq, (size_t) ack);
	
	dump_hex_block(p->data, p->len);
	
	ip_pkt_free(p);
}

	static void
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

	p->hdr_len = sizeof(struct udp_hdr);
	
	struct connection *c;

	c = find_connection(net, NET_UDP, 
			port_dst, port_src, 
			p->src_ipv4);

	if (c == nil) {
		log(LOG_INFO, "got unhandled packet");
		ip_pkt_free(p);
		return;
	}

	struct ip_pkt **o;
	for (o = &c->ip.waiting_pkts; *o != nil; o = &(*o)->next) {
		log(LOG_INFO, "after pkt %i", (*o)->id);
	}
		;

	p->next = nil;
	*o = p;
}

static struct ip_pkt *
ip_pkt_new(uint8_t *src, 
		uint8_t proto,
		uint16_t ident)
{
	struct ip_pkt *p;

	p = malloc(sizeof(struct ip_pkt));
	if (p == nil) {
		return nil;
	}

	memcpy(p->src_ipv4, src, 4);
	p->protocol = proto;
	p->id = ident;
	p->hdr_len = 0;
	p->have_last = false;
	p->len = 0;
	p->data = nil;
	p->frags = nil;
	p->next = nil;

	return p;
}

static void
ip_pkt_free(struct ip_pkt *p)
{
	struct ip_pkt_frag *a, *n;

	a = p->frags; 
	while (a != nil) {
		n = a->next;
		memcpy(p->data + a->offset, a->data, a->len);

		free(a->data);
		free(a);

		a = n;
	}

	if (p->data != nil) {
		free(p->data);
	}

	free(p);
}

static struct ip_pkt *
get_waiting_ip_pkt(struct net_dev *net,
		uint8_t *src, uint8_t proto,
		uint16_t ident)
{
	struct net_dev_internal *i = net->internal;
	struct ip_pkt *p;

	for (p = i->ip.pkts; p != nil; p = p->next) {
		if (memcmp(p->src_ipv4, src, 4)
			&& p->id == ident
			&& p->protocol == proto) 
		{
			return p;
		}
	}

	p = ip_pkt_new(src, proto, ident);
	if (p == nil) {
		return nil;
	}

	p->next = i->ip.pkts;
	i->ip.pkts = p;

	return p;
}

static bool
insert_pkt_frag(struct ip_pkt *p, struct ip_pkt_frag *f, bool have_last)
{
	struct ip_pkt_frag **o, *a;

	o = &p->frags;
	while (true) {
		if (*o == nil || f->offset < (*o)->offset) {
			f->next = *o;
			*o = f;
			log(LOG_INFO, "found spot");
			break;
		}

		o = &(*o)->next;
	}

	if (have_last) {
		p->have_last = true;
	}

	if (p->len < f->offset + f->len) {
		p->len = f->offset + f->len;
	}

	if (!p->have_last) {
		log(LOG_INFO, "don't have last");
		return false;
	} 

	for (a = p->frags; a->next != nil; a = a->next) {
		if (a->offset + a->len < a->next->offset) {
			log(LOG_INFO, "found gap");
			return false;
		}
	}

	p->data = malloc(p->len);
	if (p->data == nil) {
		log(LOG_WARNING, "out of mem");
		return false;
	}

	return true;
}

	void
handle_ipv4(struct net_dev *net, 
		struct eth_hdr *eth_hdr, 
		uint8_t *bdy, size_t len)
{
	struct ipv4_hdr *ip_hdr;
	uint8_t *ip_bdy;

	uint8_t version, hdr_len;
	size_t ip_len;

	uint16_t ident;
	uint8_t frag_flag_dont, frag_flag_more;
	uint16_t frag_offset;
	uint8_t protocol;

	ip_hdr = (void *) bdy;

	if (!memcmp(eth_hdr->dst, net->mac, 6)) {
		return;
	}

	if (!memcmp(ip_hdr->dst, net->ipv4, 4)) {
		log(LOG_INFO, "for our mac but not our ip address?");
		return;
	}

	hdr_len = (ip_hdr->ver_len & 0xf) << 2;
	version = (ip_hdr->ver_len & 0xf0) >> 4;

	if (version != 4) {
		log(LOG_INFO, "pkt has bad version %i", version);
		return;
	}

	ip_bdy = bdy + hdr_len;
	ip_len = ((ip_hdr->length[0] << 8) | ip_hdr->length[1]) - hdr_len;

	ident = ip_hdr->ident[0] << 8 | ip_hdr->ident[1];

	log(LOG_INFO, "pkt id 0x%x", ident);
	log(LOG_INFO, "ttl %i", ip_hdr->ttl);

	frag_flag_dont = (ip_hdr->fragment[0] >> 6) & 1;
	frag_flag_more = (ip_hdr->fragment[0] >> 5) & 1;
	frag_offset = ((ip_hdr->fragment[0] & 0x1f ) << 5)
		| ip_hdr->fragment[1];

	log(LOG_INFO, "frag d %i m %i offset 0x%x", 
			frag_flag_dont, frag_flag_more, frag_offset);
	
	protocol = ip_hdr->protocol;
	log(LOG_INFO, "proto %i", protocol);

	struct ip_pkt *p;

	if (frag_offset > 0 || frag_flag_more) {
		struct ip_pkt_frag *f;

		f = malloc(sizeof(struct ip_pkt_frag));
		if (f == nil) {
			return;
		}

		f->offset = frag_offset;
		f->len = ip_len;

		f->data = malloc(ip_len);
		if (f->data == nil) {
			free(f);
			return;
		}

		memcpy(f->data, ip_bdy, ip_len);

		p = get_waiting_ip_pkt(net, ip_hdr->src, protocol, ident);
		if (p == nil) {
			free(f->data);
			free(f);
			return;
		}

		if (!insert_pkt_frag(p, f, !frag_flag_more)) {
			log(LOG_WARNING, "need to wait for more");
			return;
		}

	} else {
		p = ip_pkt_new(ip_hdr->src, protocol, ident);
		if (p == nil) {
			return;
		}

		p->data = malloc(ip_len);
		if (p->data == nil) {
			free(p);
			return;
		}

		memcpy(p->data, ip_bdy, ip_len);
		p->len = ip_len;
	}
		
	switch (protocol) {
		case IP_ICMP:
			handle_icmp(net, p); 
			break;

		case IP_TCP:
			handle_tcp(net, p);
			break;

		case IP_UDP:
			handle_udp(net, p);
			break;

		default:
			break;
	}
}

static void
finish_open_udp(struct net_dev *net,
		void *arg,
		uint8_t *mac)
{
	struct connection *c = arg;
	union net_rsp rp;

	char mac_str[32];
	print_mac(mac_str, mac);

	log(LOG_INFO, "got mac dst %s", mac_str);

	memcpy(c->ip.mac_rem, mac, 6);

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

	log(LOG_INFO, "open udp");

	c->ip.port_loc = i->n_free_port++;;
	c->ip.port_rem = rq->open.port;

	memcpy(c->ip.ip_rem, rq->open.addr, 4);

	c->ip.offset_into_waiting = 0;
	c->ip.waiting_pkts = nil;

	arp_request(net, c->ip.ip_rem, &finish_open_udp, c);
}

void
ip_open_tcp(struct net_dev *net,
		union net_req *rq, 
		struct connection *c)
{

}

void
ip_write_udp(struct net_dev *net,
		int from,
		union net_req *rq, 
		struct connection *c)
{
	union net_rsp rp;

	log(LOG_INFO, "write udp");

	uint8_t *pkt, *data; 
	struct ipv4_hdr *ipv4_hdr;
	struct udp_hdr *udp_hdr;

	if (!create_udp_pkt(net,
			c->ip.mac_rem, c->ip.ip_rem,
			c->ip.port_loc, c->ip.port_rem,
			rq->write.w_len,
			&pkt, &ipv4_hdr, &udp_hdr,
			&data))
	{
		log(LOG_INFO, "create pkt fail");
		return;
	}

	uint8_t *va;

	va = map_addr(rq->write.pa, rq->write.len, MAP_RO);
	if (va == nil) {
		return;
	}

	memcpy(data, va, rq->write.w_len);

	log(LOG_INFO, "sending");

	send_udp_pkt(net, pkt, ipv4_hdr, udp_hdr, rq->write.w_len);

	log(LOG_INFO, "sent");
	free(pkt);

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
ip_write_tcp(struct net_dev *net,
		int from,
		union net_req *rq, 
		struct connection *c)
{

}

void
ip_read_udp(struct net_dev *net,
		int from,
		union net_req *rq, 
		struct connection *c)
{
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
	while (c->ip.waiting_pkts != nil && r_len < rq->read.r_len) {
		p = c->ip.waiting_pkts;

		log(LOG_INFO, "read from pkt 0x%x", p->id);

		o = c->ip.offset_into_waiting;
		log(LOG_INFO, "o = %i", o);

		l = rq->read.r_len - r_len;
		log(LOG_INFO, "l = %i", l);
		if (l > p->len - o - p->hdr_len) {
			l = p->len - o - p->hdr_len;
			log(LOG_INFO, "l cut to = %i", l);
		}

		log(LOG_INFO, "read %i bytes from %i offset", l, o);
		memcpy(va + r_len, 
			p->data + p->hdr_len + o,
		 	l);

		if (p->hdr_len + o + l < p->len) {
			log(LOG_INFO, "update offset to %i", o + l);
			c->ip.offset_into_waiting = o + l;
		} else {
			log(LOG_INFO, "shift to next pkt");
			c->ip.offset_into_waiting = 0;
			c->ip.waiting_pkts = p->next;
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

void
ip_read_tcp(struct net_dev *net,
		int from,
		union net_req *rq, 
		struct connection *c)
{

}





