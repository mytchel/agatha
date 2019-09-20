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

size_t
csum_ip_continue(uint8_t *h, size_t len, size_t sum)
{
	size_t v;
	size_t i;

	for (i = 0; i < len; i += 2) {
		v = (h[i] << 8);
		if (i + 1 < len) v |= h[i+1];

		sum += v;
	}

	return sum;
}

size_t
csum_ip_finish(size_t sum)
{
	size_t s;
	while (sum > 0xffff) {
		s = sum >> 16;
		sum = (sum & 0xffff) + s;
	}
	
	return (~sum) & 0xffff;
}

size_t 
csum_ip(uint8_t *h, size_t len)
{
	size_t sum;

	sum = 0;
	sum = csum_ip_continue(h, len, sum);
	sum = csum_ip_finish(sum);
	
	return sum;
}

bool
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

	void
send_ipv4_pkt(struct net_dev *net,
		uint8_t *pkt,
		struct ipv4_hdr *ip_hdr, 
		size_t hdr_len,
		size_t data_len)
{
	send_eth_pkt(net, pkt, hdr_len + data_len);
}

struct binding *
find_binding_ip(struct net_dev *net, 
		int proto,
		uint16_t port_loc)
{
	struct net_dev_internal *i = net->internal;
	struct binding_ip *ip;
	struct binding *b;

	for (b = i->bindings; b != nil; b = b->next) {
		ip = b->proto_arg;
		if (proto == b->proto && port_loc == ip->port_loc) {
			return b;
		}
	}

	return nil;
}

struct ip_pkt *
ip_pkt_new(uint8_t src[4], uint8_t dst[4],
		uint8_t proto,
		uint16_t ident)
{
	struct ip_pkt *p;

	p = malloc(sizeof(struct ip_pkt));
	if (p == nil) {
		return nil;
	}

	memcpy(p->src_ipv4, src, 4);
	memcpy(p->dst_ipv4, dst, 4);

	p->protocol = proto;
	p->id = ident;

	p->have_last = false;
	p->len = 0;
	p->data = nil;
	p->frags = nil;
	p->next = nil;

	return p;
}

void
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
		uint8_t *src, uint8_t *dst, uint8_t proto,
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

	p = ip_pkt_new(src, dst, proto, ident);
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

	frag_flag_dont = (ip_hdr->fragment[0] >> 6) & 1;
	frag_flag_more = (ip_hdr->fragment[0] >> 5) & 1;
	frag_offset = ((ip_hdr->fragment[0] & 0x1f ) << 5)
		| ip_hdr->fragment[1];

	if (frag_offset > 0 || frag_flag_more) {
		log(LOG_INFO, "frag d %i m %i offset 0x%x", 
				frag_flag_dont, frag_flag_more, frag_offset);
	}
	
	protocol = ip_hdr->protocol;

	struct ip_pkt *p;

	if (frag_offset > 0 || frag_flag_more) {
		struct ip_pkt_frag *f;

		f = malloc(sizeof(struct ip_pkt_frag));
		if (f == nil) {
			log(LOG_WARNING, "out of mem");
			return;
		}

		f->offset = frag_offset;
		f->len = ip_len;

		f->data = malloc(ip_len);
		if (f->data == nil) {
			log(LOG_WARNING, "out of mem");
			free(f);
			return;
		}

		memcpy(f->data, ip_bdy, ip_len);

		p = get_waiting_ip_pkt(net, 
				ip_hdr->src, ip_hdr->dst,
				protocol, ident);
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
		p = ip_pkt_new(ip_hdr->src, ip_hdr->dst, 
				protocol, ident);
		if (p == nil) {
			log(LOG_WARNING, "out of mem");
			return;
		}

		p->data = malloc(ip_len);
		if (p->data == nil) {
			log(LOG_WARNING, "out of mem");
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

int
ip_bind(struct net_dev *net,
	union net_req *rq,
	struct binding *b)
{
	struct binding_ip *ip;

	if (find_binding_ip(net, rq->bind.proto,
			rq->bind.port_loc) != nil)
	{
		log(LOG_WARNING, "port %i already bound");
		return ERR;
	}

	ip = malloc(sizeof(struct binding_ip));
	if (ip == nil) {
		return ERR;
	}

	b->proto_arg = ip;

	memcpy(ip->addr_loc, rq->bind.addr_loc, 4);
	ip->port_loc = rq->bind.port_loc;

	switch (rq->bind.proto) {
	default:
		return ERR;

	case NET_proto_udp:
		return ip_bind_udp_init(net, rq, b);

	case NET_proto_tcp:
		return ip_bind_tcp_init(net, rq, b);
	}
}

int
ip_unbind(struct net_dev *net,
	union net_req *rq,
	struct binding *b)
{
	int ret;

	switch (b->proto) {
	default:
		return ERR;

	case NET_proto_udp:
		ret = ip_unbind_udp(net, rq, b);
		break;

	case NET_proto_tcp:
		ret = ip_unbind_tcp(net, rq, b);
		break;
	}

	return ret;
}

