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
#include "ip.h"

	static bool
send_udp_pkt(struct net_dev *net,
		struct binding *b,
		uint8_t mac_rem[6],
		uint8_t addr_rem[4],
		uint16_t port_rem,
		uint8_t *data,
		size_t data_len)
{
	struct binding_ip *ip = b->proto_arg;
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

	p->src_port = port_src;

	dump_hex_block(data, data_len);

	struct binding *b;

	b = find_binding_ip(net, NET_proto_udp, port_dst);
	if (b == nil) {
		log(LOG_INFO, "got unhandled packet");
		ip_pkt_free(p);
		return;
	}

	struct binding_ip *ip = b->proto_arg;

	struct ip_pkt **o;
	for (o = &ip->udp.recv_wait; *o != nil; o = &(*o)->next)
		;

	p->next = nil;
	*o = p;
}

int
ip_bind_udp_init(struct net_dev *net,
		union net_req *rq, 
		struct binding *b)
{
	struct binding_ip *ip = b->proto_arg;

	ip->udp.offset_into_waiting = 0;
	ip->udp.recv_wait = nil;

	return OK;
}

int
ip_unbind_udp(struct net_dev *net,
		union net_req *rq, 
		struct binding *b)
{
	struct binding_ip *ip = b->proto_arg;
	struct ip_pkt *p;

	while (ip->udp.recv_wait != nil) {
		p = ip->udp.recv_wait;

		ip->udp.recv_wait = p->next;
		ip_pkt_free(p);
	}

	return OK;
}

struct udp_write_req {
	struct net_dev *net;
	struct binding *binding;
	int cap;
	uint8_t *va;
	size_t len;
	uint8_t addr_rem[4];
	uint16_t port_rem;
};

static void
udp_write_finish(struct net_dev *net, void *arg, uint8_t *mac)
{
	struct udp_write_req *r = arg;
	union net_rsp rp;

	if (!send_udp_pkt(r->net, r->binding, 
			mac,
			r->addr_rem, 
			r->port_rem,
			r->va, r->len))
	{
		log(LOG_INFO, "create pkt fail");
		return;
	}

	log(LOG_INFO, "udp sent");

	unmap_addr(r->cap, r->va);

	rp.write.type = NET_write;
	rp.write.ret = OK;
	rp.write.chan_id = r->binding->id;
	rp.write.len = r->len;

	reply_cap(net->mount_eid, r->binding->proc_id, &rp, r->cap);

	free(r);
}

void
add_udp_write_req(struct net_dev *net,
		struct binding *b,
		int cap,
		uint8_t *va,
		size_t len,
		uint8_t addr_rem[4],
		uint16_t port_rem)
{
	struct udp_write_req *r;

	r = malloc(sizeof(struct udp_write_req));
	if (r == nil) {
		log(LOG_WARNING, "out of mem");
		return;
	}

	r->net = net;
	r->binding = b;
	r->cap = cap;
	r->va = va;
	r->len = len;
	r->port_rem = port_rem;
	memcpy(r->addr_rem, addr_rem, 4);

	arp_request(net, addr_rem,
		&udp_write_finish,
		r);
}

void
ip_write_udp(struct net_dev *net,
		union net_req *rq, int cap, 
		struct binding *b)
{
	log(LOG_INFO, "write udp");

	uint8_t *va;

	va = frame_map_anywhere(cap);
	if (va == nil) {
		return;
	}

	add_udp_write_req(net, b, cap,
			va, rq->write.len,
			rq->write.proto.udp.addr_rem,
			rq->write.proto.udp.port_rem);
}

void
ip_read_udp(struct net_dev *net,
		union net_req *rq, int cap,
		struct binding *b)
{
	struct binding_ip *ip = b->proto_arg;
	size_t len, l, o;
	struct ip_pkt *p;
	union net_rsp rp;
	uint8_t *va;

	log(LOG_INFO, "read udp");

	rp.read.type = NET_read;
	rp.read.chan_id = b->id;
	rp.read.len = 0;
	memset(&rp.read.proto, 0, sizeof(rp.read.proto));
	
	va = frame_map_anywhere(cap);
	if (va == nil) {
		rp.read.ret = ERR;
		reply_cap(net->mount_eid, b->proc_id, &rp, cap);
		return;
	}

	len = 0;

	p = ip->udp.recv_wait;
	if (p != nil) {
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

		rp.read.proto.udp.port_rem = p->src_port;
		memcpy(rp.read.proto.udp.addr_rem, p->src_ipv4, 4);

		if (o + l < p->len - sizeof(struct udp_hdr)) {
			log(LOG_INFO, "update offset to %i", o + l);
			ip->udp.offset_into_waiting = o + l;
		} else {
			log(LOG_INFO, "shift to next pkt");
			ip->udp.offset_into_waiting = 0;
			ip->udp.recv_wait = p->next;
			ip_pkt_free(p);
		}
		
		len = l;
	}

	unmap_addr(cap, va);

	log(LOG_INFO, "got %i bytes in total", len);

	rp.read.ret = OK;
	rp.read.len = len;

	reply_cap(net->mount_eid, b->proc_id, &rp, cap);
}

