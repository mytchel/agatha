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

	static void
send_tcp_pkt(struct net_dev *net,
		struct connection *c)
{
	struct connection_ip *ip = c->proto_arg;
	size_t length, hdr_len, data_len;
	uint8_t *pkt, *pkt_data, *data;
	struct ipv4_hdr *ipv4_hdr;
	struct tcp_hdr *tcp_hdr;
	uint32_t seq, ack;
	uint16_t flags;
	size_t csum;

	hdr_len = sizeof(struct tcp_hdr);
	flags = (hdr_len / 4) << 12;

	struct tcp_pkt *p;

	p = ip->tcp.sending;
	if (p == nil) {
		log(LOG_WARNING, "nothing to send?");
		return;
	}

	seq = p->seq;

	switch (ip->tcp.state) {
	case TCP_state_syn_sent:
		flags |= TCP_flag_syn;
		ack = 0;
		break;

	case TCP_state_syn_received:
		log(LOG_INFO, "sending ack switch to established");
		ip->tcp.state = TCP_state_established;
	case TCP_state_established:
		ack = ip->tcp.ack + 1;
		flags |= TCP_flag_ack;
		break;

	default:
		log(LOG_INFO, "bad state %i?", ip->tcp.state);
		return;
	}

	data_len = p->len;
	data = p->data;
	
	log(LOG_INFO, "seq = 0x%x", seq);
	log(LOG_INFO, "ack = 0x%x", ack);

	hdr_len = sizeof(struct tcp_hdr);
	length = hdr_len + data_len;

	if (!create_ipv4_pkt(net, 
				ip->mac_rem, ip->ip_rem,
				20, IP_TCP,
				length,
				&pkt,
				&ipv4_hdr, 
				(uint8_t **) &tcp_hdr)) 
	{
		return;
	}
	
	tcp_hdr->port_src[0] = (ip->port_loc >> 8) & 0xff;
	tcp_hdr->port_src[1] = (ip->port_loc >> 0) & 0xff;
	tcp_hdr->port_dst[0] = (ip->port_rem >> 8) & 0xff;
	tcp_hdr->port_dst[1] = (ip->port_rem >> 0) & 0xff;
		
	tcp_hdr->seq[0] = (seq >> 24) & 0xff;
	tcp_hdr->seq[1] = (seq >> 16) & 0xff;
	tcp_hdr->seq[2] = (seq >>  8) & 0xff;
	tcp_hdr->seq[3] = (seq >>  0) & 0xff;

	tcp_hdr->ack[0] = (ack >> 24) & 0xff;
	tcp_hdr->ack[1] = (ack >> 16) & 0xff;
	tcp_hdr->ack[2] = (ack >>  8) & 0xff;
	tcp_hdr->ack[3] = (ack >>  0) & 0xff;

	tcp_hdr->flags[0] = (flags >> 8) & 0xff;
	tcp_hdr->flags[1] = (flags >> 0) & 0xff;

	tcp_hdr->win_size[0] = (ip->tcp.window_size_loc >> 8) & 0xff;
	tcp_hdr->win_size[1] = (ip->tcp.window_size_loc >> 1) & 0xff;

	tcp_hdr->csum[0] = 0;
	tcp_hdr->csum[1] = 0;
	
	tcp_hdr->urg[0] = 0;
	tcp_hdr->urg[1] = 0;

	if (data_len > 0) {
		pkt_data = ((uint8_t *) tcp_hdr) + hdr_len;
	
		memcpy(pkt_data, data, data_len);
	}

	uint8_t pseudo_hdr[12];

	memcpy(&pseudo_hdr[0], net->ipv4, 4);
	memcpy(&pseudo_hdr[4], ip->ip_rem, 4);
	pseudo_hdr[8] = 0;
	pseudo_hdr[9] = IP_TCP;
	pseudo_hdr[10] = (length >> 8) & 0xff;
	pseudo_hdr[11] = (length >> 0) & 0xff;

	csum = 0;
	csum = csum_ip_continue(pseudo_hdr, sizeof(pseudo_hdr), csum);
	csum = csum_ip_continue((uint8_t *) tcp_hdr, length, csum);
	csum = csum_ip_finish(csum);

	tcp_hdr->csum[0] = (csum >> 8) & 0xff;
	tcp_hdr->csum[1] = (csum >> 0) & 0xff;

	send_ipv4_pkt(net, pkt, ipv4_hdr, 
				20, length);

	free(pkt);
}

void
add_ack(struct net_dev *net,
		struct connection_ip *ip)
{
	struct tcp_pkt *t;
	
	t = malloc(sizeof(struct tcp_pkt));
	if (t == nil) {
		log(LOG_WARNING, "out of mem");
		return;
	}

	log(LOG_INFO, "add ack");
	t->data = nil;
	t->len = 0;
	t->seq = ip->tcp.next_seq;

	t->next = ip->tcp.sending;
	ip->tcp.sending = t;
}

static void
insert_received(struct net_dev *net,
		struct connection_ip *ip,
		uint32_t seq,
		uint8_t *data,
		size_t len)
{
	struct tcp_pkt *t, **o;
	uint32_t cont_seq;
	bool gap, already_have;

	log(LOG_INFO, "insert 0x%x of len %i", seq, len);

	already_have = false;
	gap = false;
	cont_seq = ip->tcp.ack;
	for (o = &ip->tcp.receiving; *o != nil; o = &(*o)->next) {
		log(LOG_INFO, "compare with 0x%x", (*o)->seq);
		if (!gap && cont_seq + 1 == (*o)->seq) {
			log(LOG_INFO, "inc cont");
			cont_seq++;
		} else {
			log(LOG_INFO, "gap");
			gap = true;
		}

		if (seq == (*o)->seq) {
			log(LOG_INFO, "already have pkt 0x%x", seq);
			already_have = true;
			break;
		} else if (seq < (*o)->seq) {
			log(LOG_INFO, "found spot");
			break;
		}
	}

	if (!gap && cont_seq + 1 == seq) {
		log(LOG_INFO, "inc cont");
		cont_seq++;
	}

	/* todo, shouldn't update till user reads the data */
	log(LOG_INFO, "have cont from 0x%x to 0x%x", ip->tcp.ack, cont_seq);
	if (ip->tcp.ack < cont_seq) {
		log(LOG_INFO, "got more than before, increase seq");
		ip->tcp.next_seq++;
	}

	ip->tcp.ack = cont_seq;
	add_ack(net, ip);

	if (already_have) {
		return;
	}

	t = malloc(sizeof(struct tcp_pkt));
	if (t == nil) {
		log(LOG_WARNING, "out of mem");
		return;
	}

	if (len > 0) {
		log(LOG_INFO, "copy");
		t->data = malloc(len);
		if (t->data == nil) {
			log(LOG_WARNING, "out of mem");
			free(t);
			return;
		}

		memcpy(t->data, data, len);
	} else {
		t->data = nil;
	}

	t->len = len;
	t->seq = seq;

	t->next = *o;
	*o = t;

	log(LOG_INFO, "added");
}

void
handle_tcp(struct net_dev *net, 
		struct ip_pkt *p)
{
	struct tcp_hdr *tcp_hdr;
	uint16_t port_src, port_dst;
	uint32_t seq, ack;
	uint16_t hdr_len, flags;
	struct tcp_pkt *t;
	size_t csum;

	tcp_hdr = (void *) p->data;

	port_src = tcp_hdr->port_src[0] << 8 | tcp_hdr->port_src[1];
	port_dst = tcp_hdr->port_dst[0] << 8 | tcp_hdr->port_dst[1];
	csum = tcp_hdr->csum[0] << 8 | tcp_hdr->csum[1];
	
	seq = tcp_hdr->seq[0] << 24 | tcp_hdr->seq[1] << 16 
		| tcp_hdr->seq[2] << 8 | tcp_hdr->seq[3];

	ack = tcp_hdr->ack[0] << 24 | tcp_hdr->ack[1] << 16 
		| tcp_hdr->ack[2] << 8 | tcp_hdr->ack[3];

	hdr_len = (tcp_hdr->flags[0] >> 4) * 4;
	flags = ((tcp_hdr->flags[0] & 0xf) << 8) | (tcp_hdr->flags[1] << 0);

	log(LOG_INFO, "tdp packet from port %i to port %i",
			(size_t) port_src, (size_t) port_dst);
	log(LOG_INFO, "tdp packet csum 0x%x seq %x ack 0x%x",
			(size_t) csum, (size_t) seq, (size_t) ack);
	log(LOG_INFO, "tdp packet hdr len %i flags 0x%x",
			(size_t) hdr_len, (size_t) flags);

	dump_hex_block(p->data, p->len);

	p->hdr_len = hdr_len;

	struct connection *c;

	c = find_connection_ip(net, NET_TCP, 
			port_dst, port_src, 
			p->src_ipv4);

	if (c == nil) {
		log(LOG_INFO, "got unhandled packet");
		ip_pkt_free(p);
		return;
	}

	struct connection_ip *ip = c->proto_arg;

	if (flags & TCP_flag_ack) {
		while (ip->tcp.sending != nil) {
			t = ip->tcp.sending;
			if (ack < t->seq) {
				break;
			} else {
				log(LOG_INFO, "pkt 0x%x acked", t->seq);
				ip->tcp.sending = t->next;
				if (t->data != nil) 
					free(t->data);
				free(t);
			}
		}
	}

	if (flags & TCP_flag_syn) {
		log(LOG_INFO, "got syn, state = %i", ip->tcp.state);
		if (ip->tcp.state == TCP_state_syn_sent) {
			log(LOG_INFO, "switch to received");
			ip->tcp.state = TCP_state_syn_received;
			ip->tcp.ack = seq;

			log(LOG_INFO, "load next with seq 0x%x", ip->tcp.next_seq);

			add_ack(net, ip);
			ip->tcp.next_seq++;

		} else {
			log(LOG_INFO, "why? havent been sending");
		}

	} else if (flags & TCP_flag_ack) {
		if (seq > ip->tcp.ack) {
			log(LOG_INFO, "have new");
			
			insert_received(net, ip, seq, 
				p->data + hdr_len, 
				p->len - hdr_len);

		} else {
			log(LOG_INFO, "did they miss an ack?");
			add_ack(net, ip);
		}
	}

	send_tcp_pkt(net, c);

	ip_pkt_free(p);
}

static void
open_tcp_arp(struct net_dev *net,
		void *arg,
		uint8_t *mac)
{
	struct connection *c = arg;
	struct connection_ip *ip = c->proto_arg;
	
	char mac_str[32];
	print_mac(mac_str, mac);

	log(LOG_INFO, "got mac dst %s", mac_str);

	memcpy(ip->mac_rem, mac, 6);

	send_tcp_pkt(net, c);
}

void
ip_open_tcp(struct net_dev *net,
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

	log(LOG_INFO, "open tcp");

	ip->port_loc = i->n_free_port++;;
	ip->port_rem = rq->open.port;

	memcpy(ip->ip_rem, rq->open.addr, 4);

	ip->offset_into_waiting = 0;
	ip->recv_wait = nil;

	ip->tcp.state = TCP_state_syn_sent;

	ip->tcp.next_seq = 0xaaaaa;
	ip->tcp.sending = nil;
	
	ip->tcp.ack = 0;
	ip->tcp.receiving = nil;

	ip->tcp.window_size_loc = 1024;
	ip->tcp.window_size_rem = 0;
	ip->tcp.window_sent = 0;

	struct tcp_pkt *t;

	t = malloc(sizeof(struct tcp_pkt));
	if (t == nil) {
		log(LOG_WARNING, "out of mem");
		return;
	}

	t->data = nil;
	t->len = 0;
	t->seq = ip->tcp.next_seq++;
	t->next = nil;

	ip->tcp.sending = t;

	arp_request(net, ip->ip_rem, &open_tcp_arp, c);
}

void
ip_write_tcp(struct net_dev *net,
		int from,
		union net_req *rq, 
		struct connection *c)
{

}

void
ip_read_tcp(struct net_dev *net,
		int from,
		union net_req *rq, 
		struct connection *c)
{

}

void
ip_close_tcp(struct net_dev *net,
		int from,
		union net_req *rq, 
		struct connection *c)
{
}


