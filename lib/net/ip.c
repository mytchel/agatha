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
				20, 0x01,
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
				20, 0x11,
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
		uint8_t *src_mac, uint8_t *src_ipv4, 
		struct icmp_hdr *icmp_hdr,
		uint8_t *bdy,
		size_t bdy_len)
{
	struct ipv4_hdr *ip_hdr_r;
	struct icmp_hdr *icmp_hdr_r;
	uint8_t *pkt, *bdy_r;

	/* icmp */

	log(LOG_INFO, "send echo reply");

	if (!create_icmp_pkt(net, src_mac, src_ipv4,
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
		uint8_t *src_mac, uint8_t *src_ipv4,
		uint8_t *bdy,
		size_t bdy_len)
{
	struct icmp_hdr *icmp_hdr;

	icmp_hdr = (void *) bdy;

	switch (icmp_hdr->type) {
		case 0:
			/* echo reply */
			break;

		case 8:
			/* echo request */
			handle_echo_request(net,
					src_mac, src_ipv4,
					icmp_hdr,
					bdy + sizeof(struct icmp_hdr),
					bdy_len - sizeof(struct icmp_hdr));
			break;

		default:
			log(LOG_INFO, "other icmp type %i", icmp_hdr->type);
			break;
	}
}

	static void
handle_tcp(struct net_dev *net, 
		uint8_t *src_mac, uint8_t *src_ipv4,
		uint8_t *bdy, 
		size_t bdy_len)
{
	struct tcp_hdr *tcp_hdr;
	uint16_t port_src, port_dst;
	uint32_t seq, ack;
	size_t csum;

	tcp_hdr = (void *) bdy;

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
	
	dump_hex_block(bdy, bdy_len);
}

	static void
handle_udp(struct net_dev *net, 
		uint8_t *src_mac, uint8_t *src_ipv4,
		uint8_t *bdy,
		size_t bdy_len)
{
	struct udp_hdr *udp_hdr;
	uint16_t port_src, port_dst;
	size_t length, csum, data_len;
	uint8_t *data;

	udp_hdr = (void *) bdy;

	port_src = udp_hdr->port_src[0] << 8 | udp_hdr->port_src[1];
	port_dst = udp_hdr->port_dst[0] << 8 | udp_hdr->port_dst[1];
	length = udp_hdr->length[0] << 8 | udp_hdr->length[1];
	csum = udp_hdr->csum[0] << 8 | udp_hdr->csum[1];

	log(LOG_INFO, "udp packet from port %i to port %i",
			(size_t) port_src, (size_t) port_dst);

	if (length > bdy_len) {
		log(LOG_INFO, "udp packet length %i larger than packet len %i",
				length, bdy_len);
		return;
	}

	data = bdy + sizeof(struct udp_hdr);
	data_len = length - sizeof(struct udp_hdr);

	log(LOG_INFO, "udp packet length %i csum 0x%x",
			data_len, csum);

	dump_hex_block(data, data_len);

	struct ipv4_hdr *ip_hdr_r;
	struct udp_hdr *udp_hdr_r;
	uint8_t *pkt, *bdy_r;

	size_t bdy_r_len = 6;

	if (!create_udp_pkt(net, 
				src_mac, src_ipv4,
				port_dst, port_src,
				bdy_r_len,
				&pkt, &ip_hdr_r, &udp_hdr_r, &bdy_r)) {
		return;
	}

	bdy_r[0] = 'w';
	bdy_r[1] = 'h';
	bdy_r[2] = 'a';
	bdy_r[3] = 't';
	bdy_r[4] = '?';
	bdy_r[5] = '\n';

	send_udp_pkt(net, pkt, ip_hdr_r, udp_hdr_r, bdy_r_len);
	free(pkt);
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

	if (frag_flag_more || frag_offset > 0) {
		log(LOG_INFO, "fragmented packet offset 0x%x", 
				frag_offset);

		return;
	}

	switch (protocol) {
		case 0x01:
			handle_icmp(net, 
					eth_hdr->src, ip_hdr->src, 
					ip_bdy, ip_len);
			break;

		case 0x06:
			handle_tcp(net, 
					eth_hdr->src, ip_hdr->src, 
					ip_bdy, ip_len);
			break;

		case 0x11:
			handle_udp(net, 
					eth_hdr->src, ip_hdr->src, 
					ip_bdy, ip_len);
			break;

		default:
			break;
	}
}

