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

	while (true) {
		p = ip->tcp.sending;
		if (p == nil) {
			log(LOG_WARNING, "nothing to send?");
			return;
		} else if (p->flags & TCP_flag_syn) {
			break;
		} else if (p->len > 0) {
			break;
		} else if(p->next == nil) {
			break;
		} else if (!(p->flags & TCP_flag_fin) && !(p->flags & TCP_flag_syn)) {
			log(LOG_INFO, "remove empty pkt 0x%x", p->seq);
			ip->tcp.sending = p->next;
			free(p);
		} else {
			break;
		}
	}

	seq = p->seq;
	ack = ip->tcp.ack;

	flags |= p->flags;
	
	data = p->data;
	data_len = p->len;
	
	log(LOG_INFO, "seq = 0x%x", seq);
	log(LOG_INFO, "ack = 0x%x", ack);
	log(LOG_INFO, "len = 0x%x", data_len);

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
add_pkt(struct net_dev *net,
		struct connection_ip *ip,
		uint16_t flags,
		uint8_t *data,
		size_t len)
{
	struct tcp_pkt *t, **o;
	
	t = malloc(sizeof(struct tcp_pkt));
	if (t == nil) {
		log(LOG_WARNING, "out of mem");
		return;
	}

	t->data = data;
	t->len = len;
	t->flags = flags;
	t->seq = ip->tcp.next_seq;
	
	log(LOG_INFO, "add pkt 0x%x with flags 0x%x len 0x%x", t->seq, t->flags, t->len);

	for (o = &ip->tcp.sending; *o != nil; o = &(*o)->next)
		;

	t->next = *o;
	*o = t;
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

	/* TODO: bug because this is being done wrong.
	  cont_seq should be found and updated when user reads
	  and we remove the packets from the queue then we 
	  ack to remote. otherwise this has a bug. 
	  
	  eh, seq and ack are in bytes!

	  how will we deal with byte acks?
	  for resends of partial previous packets?
	  its a fucking stream?

	 */
	  
	already_have = false;
	gap = false;
	cont_seq = ip->tcp.ack;
	for (o = &ip->tcp.recv_wait; *o != nil; o = &(*o)->next) {
		/* will not need this */
		if (cont_seq >= (*o)->seq) continue;

		log(LOG_INFO, "compare with 0x%x 0x%x 0x%x", seq, cont_seq, (*o)->seq);
		if (!gap) {
			if (cont_seq == (*o)->seq) {
				log(LOG_INFO, "inc cont");
				cont_seq += (*o)->len;
			} else {
				log(LOG_INFO, "gap");
				gap = true;
			}
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

	if (!gap && cont_seq == seq) {
		log(LOG_INFO, "this pkt is last in last");
		cont_seq += len;
	}

#if 0
	/* todo, shouldn't update till user reads the data */
	log(LOG_INFO, "have cont from 0x%x to 0x%x", ip->tcp.ack, cont_seq);
	ip->tcp.ack = cont_seq;

	add_pkt(net, ip, TCP_flag_ack, nil, 0);
#endif
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
	uint8_t *data;
	size_t data_len;

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

	log(LOG_INFO, "tcp packet from port %i to port %i",
			(size_t) port_src, (size_t) port_dst);
	log(LOG_INFO, "tcp packet csum 0x%x seq %x ack 0x%x",
			(size_t) csum, (size_t) seq, (size_t) ack);
	log(LOG_INFO, "tcp packet hdr len %i flags 0x%x",
			(size_t) hdr_len, (size_t) flags);

	dump_hex_block(p->data, p->len);

	data = p->data + hdr_len;
	data_len = p->len - hdr_len;

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
			if (ack < t->seq + t->len) {
				log(LOG_INFO, "pkt 0x%x still not acked", t->seq);
				break;
			} else {
				log(LOG_INFO, "pkt 0x%x acked", t->seq);

				if (t->flags & TCP_flag_fin) {
					switch (ip->tcp.state) {
					default:
						log(LOG_INFO, "fin pkt acked in bad state %i", ip->tcp.state);
						break;

					case TCP_state_fin_wait_1:
						log(LOG_INFO, "rem acked fin, goto wait 2");
						ip->tcp.state = TCP_state_fin_wait_2;
						ip->tcp.ack++;
						break;
					}
				}

				ip->tcp.sending = t->next;
				if (t->data != nil) 
					free(t->data);
				free(t);
			}
		}
	}

	if (flags & TCP_flag_fin) {
		log(LOG_INFO, "got fin");
		switch (ip->tcp.state) {
		default:
			log(LOG_WARNING, "got fin in state %i", ip->tcp.state);
			break;

		case TCP_state_established:
			ip->tcp.state = TCP_state_close_wait;

			if (ip->tcp.recv_wait == nil) {
				ip->tcp.ack = seq + 1;
				add_pkt(net, ip, TCP_flag_ack, nil, 0);
				ip->tcp.next_seq++;
				add_pkt(net, ip, TCP_flag_ack|TCP_flag_fin, nil, 0);
			
				ip->tcp.state = TCP_state_last_ack;
				
				send_tcp_pkt(net, c);
			} else {
				log(LOG_INFO, "havent read everything, don't respond");
			}

			break;
			
		case TCP_state_time_wait:
			log(LOG_INFO, "in time wait, got extra fin?");
		case TCP_state_fin_wait_2:
			log(LOG_INFO, "in fin wait 2, go to time wait");
			add_pkt(net, ip, TCP_flag_ack, nil, 0);
			ip->tcp.state = TCP_state_time_wait;
			send_tcp_pkt(net, c);
			break;
		}
	}

	if (flags & TCP_flag_syn) {
		log(LOG_INFO, "got syn, state = %i", ip->tcp.state);
		if (ip->tcp.state == TCP_state_syn_sent) {
			log(LOG_INFO, "switch to received");
			ip->tcp.state = TCP_state_syn_received;

			ip->tcp.ack = seq + 1;

			ip->tcp.next_seq++;

			log(LOG_INFO, "load next with seq 0x%x", ip->tcp.next_seq);

			add_pkt(net, ip, TCP_flag_ack, nil, 0);
			send_tcp_pkt(net, c);
			
		} else {
			log(LOG_INFO, "why? havent been sending");
		}

	} else if (flags & TCP_flag_ack) {
		if (ip->tcp.ack < seq + data_len) {
			log(LOG_INFO, "have new");
			
			insert_received(net, ip, seq, 
				data, data_len);

		} else if (data_len > 0) {
			log(LOG_INFO, "did they miss an ack?");
			add_pkt(net, ip, TCP_flag_ack, nil, 0);
			send_tcp_pkt(net, c);
		} else {
			log(LOG_INFO, "just ack for 0x%x", ack);
		}
	}

	ip_pkt_free(p);
}

static void
open_tcp_arp(struct net_dev *net,
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

	send_tcp_pkt(net, c);

	rp.open.type = NET_open_rsp;
	rp.open.ret = OK;
	rp.open.id = c->id;

	send(c->proc_id, &rp);
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
	ip->tcp.offset_into_waiting = 0;
	ip->tcp.recv_wait = nil;

	ip->tcp.window_size_loc = 1024;
	ip->tcp.window_size_rem = 0;
	ip->tcp.window_sent = 0;

	add_pkt(net, ip, TCP_flag_syn, nil, 0);

	arp_request(net, ip->ip_rem, &open_tcp_arp, c);
}

void
ip_write_tcp(struct net_dev *net,
		int from,
		union net_req *rq, 
		struct connection *c)
{
	struct connection_ip *ip = c->proto_arg;
	union net_rsp rp;
	uint8_t *d;

	log(LOG_INFO, "TCP write %i", rq->write.w_len);

	d = malloc(rq->write.w_len);
	if (d == nil) {
		log(LOG_WARNING, "out of mem");
		return;
	}

	uint8_t *va;

	va = map_addr(rq->write.pa, rq->write.len, MAP_RO);
	if (va == nil) {
		log(LOG_WARNING, "map failed");
		return;
	}

	memcpy(d, va, rq->write.w_len);

	unmap_addr(va, rq->write.len);

	give_addr(from, rq->write.pa, rq->write.len);

	add_pkt(net, ip, 
			TCP_flag_ack|TCP_flag_psh, 
			d, rq->write.w_len);

	ip->tcp.next_seq += rq->write.w_len;
	
	send_tcp_pkt(net, c);

	rp.write.type = NET_write_rsp;
	rp.write.ret = OK;
	rp.write.w_len = rq->write.w_len;
	rp.write.pa = rq->write.pa;
	rp.write.len = rq->write.len;

	send(from, &rp);
}

void
ip_read_tcp(struct net_dev *net,
		int from,
		union net_req *rq, 
		struct connection *c)
{
	struct connection_ip *ip = c->proto_arg;
	size_t len, o, l, cont_ack;
	struct tcp_pkt *p;
	union net_rsp rp;
	uint8_t *va;

	va = map_addr(rq->read.pa, rq->read.len, MAP_RW);
	if (va == nil) {
		log(LOG_WARNING, "map failed");
		return;
	}

	len = 0;
	cont_ack = ip->tcp.ack;
	while (ip->tcp.recv_wait != nil && len < rq->read.r_len) {
		p = ip->tcp.recv_wait;
		if (p->seq != cont_ack) {
			break;
		}

		o = ip->tcp.offset_into_waiting;
		l = rq->read.r_len - len;
		if (l > p->len - o) {
			l = p->len - o;
		}

		memcpy(va + len,
			p->data + o,
			l);

		if (o + l < p->len) {
			ip->tcp.offset_into_waiting = o + l;
		} else {
			ip->tcp.offset_into_waiting = 0;
			ip->tcp.recv_wait = p->next;
			free(p->data);
			free(p);
			cont_ack += l;
		}

		len += l;
	}

	log(LOG_INFO, "increase ack from 0x%x to 0x%x", ip->tcp.ack, cont_ack);

	ip->tcp.ack = cont_ack;
	add_pkt(net, ip, TCP_flag_ack, nil, 0);
	send_tcp_pkt(net, c);

	unmap_addr(va, rq->read.len);

	give_addr(from, rq->read.pa, rq->read.len);

	rp.read.type = NET_read_rsp;
	rp.read.ret = OK;
	rp.read.r_len = len;
	rp.read.pa = rq->read.pa;
	rp.read.len = rq->read.len;

	send(from, &rp);
}

int
ip_close_tcp(struct net_dev *net,
		int from,
		union net_req *rq, 
		struct connection *c)
{
	struct connection_ip *ip = c->proto_arg;

	log(LOG_INFO, "close req");

	ip->tcp.state = TCP_state_fin_wait_1;

	add_pkt(net, ip, TCP_flag_ack|TCP_flag_fin, nil, 0);
	send_tcp_pkt(net, c);

	return OK;
}


