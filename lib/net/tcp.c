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


/* This has many problems.
   - partial packet acks are not supported.
   - resending if we don't get an ack is not
     suported.
   - closing works from both sides but i don't think
     it is done properly.
   - sockets are never free'd.
 */

static void
connect_tcp_finish(struct net_dev *net,
		struct binding *b,
		struct tcp_con *c)
{
	struct binding_ip *ip = b->proto_arg;
	union net_rsp rp;

	log(LOG_INFO, "TCP connect finished, con %i", c->id);

	c->next = ip->tcp.cons;
	ip->tcp.cons = c;
	ip->tcp.listen_con = nil;

	rp.tcp_connect.type = NET_tcp_connect_rsp;
	rp.tcp_connect.ret = OK;
	rp.tcp_connect.chan_id = b->id;
	rp.tcp_connect.con_id = c->id;

	send(b->proc_id, &rp);
}

static void
listen_tcp_finish(struct net_dev *net,
		struct binding *b,
		struct tcp_con *c)
{
	struct binding_ip *ip = b->proto_arg;
	union net_rsp rp;

	log(LOG_INFO, "TCP listen finished, con %i", c->id);

	c->next = ip->tcp.cons;
	ip->tcp.cons = c;
	ip->tcp.listen_con = nil;

	rp.tcp_listen.type = NET_tcp_listen_rsp;
	rp.tcp_listen.ret = OK;
	rp.tcp_listen.chan_id = b->id;
	rp.tcp_listen.con_id = c->id;

	send(b->proc_id, &rp);
}

	static void
send_tcp_pkt(struct net_dev *net,
		struct binding *b,
		struct tcp_con *c)
{
	struct binding_ip *ip = b->proto_arg;

	size_t length, hdr_len, data_len;
	uint8_t *pkt, *pkt_data, *data;
	struct ipv4_hdr *ipv4_hdr;
	struct tcp_hdr *tcp_hdr;
	uint32_t seq, ack;
	uint16_t flags;
	size_t csum;

	log(LOG_INFO, "tcp SEND");

	hdr_len = sizeof(struct tcp_hdr);
	flags = (hdr_len / 4) << 12;

	struct tcp_pkt *p;

	while (true) {
		p = c->sending;
		if (p == nil) {
			log(LOG_WARNING, "nothing to send?");
			return;
		} else if (p->flags & TCP_flag_syn) {
			break;
		} else if (p->len > 0) {
			break;
		} else if (p->next == nil) {
			break;
		} else if (!(p->flags & (TCP_flag_fin | TCP_flag_syn))) {
			log(LOG_INFO, "remove empty pkt 0x%x", p->seq);
			c->sending = p->next;
			free(p);
		} else {
			break;
		}
	}

	seq = p->seq;
	ack = c->ack;

	flags |= p->flags;
	
	data = p->data;
	data_len = p->len;
	
	log(LOG_INFO, "seq = 0x%x", seq);
	log(LOG_INFO, "ack = 0x%x", ack);
	log(LOG_INFO, "len = 0x%x", data_len);

	hdr_len = sizeof(struct tcp_hdr);
	length = hdr_len + data_len;

	if (!create_ipv4_pkt(net, 
				c->mac_rem, c->addr_rem,
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
	tcp_hdr->port_dst[0] = (c->port_rem >> 8) & 0xff;
	tcp_hdr->port_dst[1] = (c->port_rem >> 0) & 0xff;
		
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

	tcp_hdr->win_size[0] = (c->window_size_loc >> 8) & 0xff;
	tcp_hdr->win_size[1] = (c->window_size_loc >> 0) & 0xff;

	tcp_hdr->csum[0] = 0;
	tcp_hdr->csum[1] = 0;
	
	tcp_hdr->urg[0] = 0;
	tcp_hdr->urg[1] = 0;

	if (data_len > 0) {
		pkt_data = ((uint8_t *) tcp_hdr) + hdr_len;
	
		memcpy(pkt_data, data, data_len);
	}

	uint8_t pseudo_hdr[12];

	memcpy(&pseudo_hdr[0], ip->addr_loc, 4);
	memcpy(&pseudo_hdr[4], c->addr_rem, 4);
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
add_pkt(struct tcp_con *c,
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
	t->seq = c->next_seq;
	
	log(LOG_INFO, "add pkt 0x%x with flags 0x%x len 0x%x", t->seq, t->flags, t->len);
	for (o = &c->sending; *o != nil; o = &(*o)->next)
		;

	t->next = *o;
	*o = t;
}

static void
insert_received(struct net_dev *net,
		struct binding *b,
		struct tcp_con *c,
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
	cont_seq = c->ack;
	for (o = &c->recv_wait; *o != nil; o = &(*o)->next) {
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
	uint8_t *data;
	size_t data_len;

	tcp_hdr = (void *) p->data;

	port_src = tcp_hdr->port_src[0] << 8 | tcp_hdr->port_src[1];
	port_dst = tcp_hdr->port_dst[0] << 8 | tcp_hdr->port_dst[1];
	
	seq = tcp_hdr->seq[0] << 24 | tcp_hdr->seq[1] << 16 
		| tcp_hdr->seq[2] << 8 | tcp_hdr->seq[3];

	ack = tcp_hdr->ack[0] << 24 | tcp_hdr->ack[1] << 16 
		| tcp_hdr->ack[2] << 8 | tcp_hdr->ack[3];

	hdr_len = (tcp_hdr->flags[0] >> 4) * 4;
	flags = ((tcp_hdr->flags[0] & 0xf) << 8) | (tcp_hdr->flags[1] << 0);

	log(LOG_INFO, "tcp RECV");
	log(LOG_INFO, "tcp packet from port %i to port %i",
			(size_t) port_src, (size_t) port_dst);
	log(LOG_INFO, "tcp packet seq 0x%x ack 0x%x",
			(size_t) seq, (size_t) ack);
	log(LOG_INFO, "tcp packet hdr len %i flags 0x%x",
			(size_t) hdr_len, (size_t) flags);

	data = p->data + hdr_len;
	data_len = p->len - hdr_len;

	struct binding *b;

	b = find_binding_ip(net, NET_proto_tcp, port_dst);

	if (b == nil) {
		log(LOG_INFO, "got unhandled packet without bind");
		ip_pkt_free(p);
		return;
	}

	struct binding_ip *ip = b->proto_arg;
	struct tcp_con *c;

	for (c = ip->tcp.cons; c != nil; c = c->next) {
		if (port_src == c->port_rem
			&& memcmp(p->src_ipv4, c->addr_rem, 4)) 
		{
			break;
		}
	}

	if (c == nil) {
		c = ip->tcp.listen_con;
		if (ip->tcp.listen_con != nil) {
			if (c->state != TCP_state_listen) {
				if (port_src != c->port_rem
					|| !memcmp(p->src_ipv4, c->addr_rem, 4)) 
				{
					log(LOG_INFO, "listening but connecting to another");
					c = nil;
				}
			} 
		} 
		
		if (c == nil) {
			log(LOG_INFO, "got unhandled packet without con");
			ip_pkt_free(p);
			return;
		}
	}

	c->window_size_rem = tcp_hdr->win_size[0] << 8 
		| tcp_hdr->win_size[1];

	if (flags & TCP_flag_ack) {
		while (c->sending != nil) {
			t = c->sending;
			if (ack < t->seq + t->len) {
				log(LOG_INFO, "pkt 0x%x still not acked", t->seq);
				break;
			} else {
				log(LOG_INFO, "pkt 0x%x acked", t->seq);

				if (t->flags & TCP_flag_fin) {
					switch (c->state) {
					default:
						log(LOG_INFO, "fin pkt acked in bad state %i", c->state);
						break;

					case TCP_state_fin_wait_1:
						log(LOG_INFO, "rem acked fin, goto wait 2");
						c->state = TCP_state_fin_wait_2;
						c->ack++;
						break;
					}
				}

				c->sending = t->next;
				if (t->data != nil) 
					free(t->data);
				free(t);
			}
		}
	}

	if (flags & TCP_flag_fin) {
		log(LOG_INFO, "got fin");
		switch (c->state) {
		default:
			log(LOG_WARNING, "got fin in state %i", c->state);
			break;

		case TCP_state_established:
			c->state = TCP_state_close_wait;

			if (c->recv_wait == nil) {	
				log(LOG_INFO, "ack fin");

				c->ack++;
				add_pkt(c, TCP_flag_ack, nil, 0);
	
				send_tcp_pkt(net, b, c);
			} else {
				log(LOG_INFO, "havent read everything, don't respond");
			}

			break;

		case TCP_state_time_wait:
			log(LOG_INFO, "in time wait, got extra fin?");
		case TCP_state_fin_wait_2:
			log(LOG_INFO, "in fin wait 2, go to time wait");
			add_pkt(c, TCP_flag_ack, nil, 0);
			c->state = TCP_state_time_wait;
			send_tcp_pkt(net, b, c);
			break;
		}

	} else if (flags & TCP_flag_syn) {
		log(LOG_INFO, "got syn");

		switch (c->state) {
		default:
			log(LOG_WARNING, "got syn in bad state %i", c->state);
			break;

		case TCP_state_syn_sent:
			log(LOG_INFO, "switch to received");
			c->state = TCP_state_syn_received;

			c->ack = seq + 1;

			c->next_seq++;

			log(LOG_INFO, "load next with seq 0x%x", c->next_seq);

			add_pkt(c, TCP_flag_ack, nil, 0);
			send_tcp_pkt(net, b, c);
			
			c->state = TCP_state_established;
			connect_tcp_finish(net, b, c);

			break;

		case TCP_state_listen:
			log(LOG_INFO, "got syn in listening");

			c->port_rem = port_src;
			memcpy(c->addr_rem, p->src_ipv4, 4);

			if (!arp_match_ip(net, c->addr_rem, c->mac_rem)) {
				log(LOG_WARNING, "got con open but dont have rem mac?!");
				ip_pkt_free(p);
				return;
			}

			c->ack = seq + 1;
			c->state = TCP_state_syn_received;

			add_pkt(c, TCP_flag_syn|TCP_flag_ack, nil, 0);
			send_tcp_pkt(net, b, c);

			break;
		}

	} else if (flags & TCP_flag_ack) {
		switch (c->state) {
		default:
			log(LOG_WARNING, "got ack in bad state %i", c->state);
			break;

		case TCP_state_last_ack:
			log(LOG_INFO, "got last ack can close now");
			break;

		case TCP_state_fin_wait_2:
			log(LOG_INFO, "got ack in fin wait 2");
			break;

		case TCP_state_syn_received:
			log(LOG_INFO, "got ack in syn recv");
			c->state = TCP_state_established;

			c->next_seq++;

			listen_tcp_finish(net, b, c);
			break;

		case TCP_state_established:
			log(LOG_INFO, "ack in est, have 0x%x, get 0x%x + 0x%x",
				c->ack, seq, data_len);

			if (c->ack < seq + data_len) {
				log(LOG_INFO, "have new");
				
				insert_received(net, b, c, seq, 
					data, data_len);

			} else if (data_len > 0) {
				log(LOG_INFO, "did they miss an ack?");
				add_pkt(c, TCP_flag_ack, nil, 0);
				send_tcp_pkt(net, b, c);
			} else {
				log(LOG_INFO, "just ack for 0x%x", ack);
			}

			break;
		}
	}

	ip_pkt_free(p);
}

int
ip_bind_tcp(struct net_dev *net,
		union net_req *rq, 
		struct binding *b)
{
	struct binding_ip *ip;

	ip = malloc(sizeof(struct binding_ip));
	if (ip == nil) {
		return ERR;
	}

	b->proto_arg = ip;

	memcpy(ip->addr_loc, rq->bind.addr_loc, 4);
	ip->port_loc = rq->bind.port_loc;

	ip->tcp.listen_con = nil;

	ip->tcp.next_con_id = 0;
	ip->tcp.cons = nil;

	return OK;
}

int
ip_unbind_tcp(struct net_dev *net,
		union net_req *rq, 
		struct binding *b)
{
	return ERR;
}

void
ip_tcp_listen(struct net_dev *net,
		union net_req *rq, 
		struct binding *b)
{
	struct binding_ip *ip = b->proto_arg;
	struct tcp_con *c;

	c = malloc(sizeof(struct tcp_con));
	if (c == nil) {
		return;
	}

	c->id = ip->tcp.next_con_id++;

	c->next = nil;
	c->port_rem = 0;
	memset(c->mac_rem, 0, 6);

	c->state = TCP_state_listen;

	c->window_size_loc = 1024;
	c->window_size_rem = 0;
	c->window_sent = 0;
	
	c->ack = 0;
	c->next_seq = 0x4321;
	c->sending = nil;
	c->offset_into_waiting = 0;
	c->recv_wait = nil;

	ip->tcp.listen_con = c;
}

static void
connect_tcp_arp(struct net_dev *net,
		void *arg,
		uint8_t *mac)
{
	struct tcp_con *c = arg;
	struct binding *b = c->binding;
	
	char mac_str[32];
	print_mac(mac_str, mac);

	log(LOG_INFO, "got mac dst %s", mac_str);

	memcpy(c->mac_rem, mac, 6);

	send_tcp_pkt(net, b, c);
}

void
ip_tcp_connect(struct net_dev *net,
		union net_req *rq, 
		struct binding *b)
{
	struct binding_ip *ip = b->proto_arg;
	struct tcp_con *c;

	log(LOG_INFO, "connect tcp");

	c = malloc(sizeof(struct tcp_con));
	if (c == nil) {
		return;
	}

	c->next = nil;
	c->binding = b;

	c->id = ip->tcp.next_con_id++;

	c->port_rem = rq->tcp_connect.port_rem;
	memset(c->mac_rem, 0, 6);
	memcpy(c->addr_rem, rq->tcp_connect.addr_rem, 4);

	c->window_size_loc = 1024;
	c->window_size_rem = 0;
	c->window_sent = 0;
	
	c->ack = 0;
	c->next_seq = 0xaaaa;
	c->sending = nil;
	c->offset_into_waiting = 0;
	c->recv_wait = nil;

	c->next = ip->tcp.cons;
	ip->tcp.cons = c;

	c->state = TCP_state_syn_sent;

	add_pkt(c, TCP_flag_syn, nil, 0);

	arp_request(net, c->addr_rem, &connect_tcp_arp, c);
}

static struct tcp_con *
find_tcp_con(struct binding *b, int id)
{
	struct binding_ip *ip = b->proto_arg;
	struct tcp_con *c;

	for (c = ip->tcp.cons; c != nil; c = c->next) {
		if (c->id == id) {
			return c;
		}
	}

	return nil;
}

void
ip_tcp_disconnect(struct net_dev *net,
		union net_req *rq, 
		struct binding *b)
{
	struct tcp_con *c;

	log(LOG_INFO, "disconnect req");

	c = find_tcp_con(b, rq->tcp_disconnect.con_id);
	if (c == nil) {
		log(LOG_WARNING, "couldn't find con %i", rq->tcp_disconnect.con_id);
		return;
	}

	/* TODO: what should happen if there is data waiting? */

	switch (c->state) {
	case TCP_state_established:
		log(LOG_INFO, "start close");
		c->state = TCP_state_fin_wait_1;
		add_pkt(c, TCP_flag_ack|TCP_flag_fin, nil, 0);
		break;
	
	case TCP_state_close_wait:
		log(LOG_INFO, "send response fin");
		c->state = TCP_state_last_ack;
		/*c->next_seq++;*/
		add_pkt(c, TCP_flag_ack|TCP_flag_fin, nil, 0);
		break;

	default:
		log(LOG_WARNING, "bad state %i", c->state);
		break;
	}

	send_tcp_pkt(net, b, c);
}

void
ip_write_tcp(struct net_dev *net,
		union net_req *rq, 
		struct binding *b)
{
	struct tcp_con *c;
	union net_rsp rp;
	uint8_t *d;

	log(LOG_INFO, "TCP write %i", rq->write.len);

	c = find_tcp_con(b, rq->write.proto.tcp.con_id);
	if (c == nil) {
		log(LOG_WARNING, "couldn't find con %i", rq->write.proto.tcp.con_id);
		return;
	}

	if (c->state != TCP_state_established) {
		log(LOG_WARNING, "bad state %i", c->state);

		give_addr(b->proc_id, rq->write.pa, rq->write.pa_len);

		rp.write.type = NET_write_rsp;
		rp.write.ret = ERR;
		rp.write.chan_id = b->id;
		rp.write.proto.tcp.con_id = c->id;
		rp.write.len = 0;
		rp.write.pa = rq->write.pa;
		rp.write.pa_len = rq->write.pa_len;

		send(b->proc_id, &rp);

		return;
	}

	d = malloc(rq->write.len);
	if (d == nil) {
		log(LOG_WARNING, "out of mem");
		return;
	}

	uint8_t *va;

	va = map_addr(rq->write.pa, rq->write.pa_len, MAP_RO);
	if (va == nil) {
		log(LOG_WARNING, "map failed");
		return;
	}

	memcpy(d, va, rq->write.len);

	unmap_addr(va, rq->write.pa_len);

	give_addr(b->proc_id, rq->write.pa, rq->write.pa_len);

	add_pkt(c, TCP_flag_ack|TCP_flag_psh, 
			d, rq->write.len);

	c->next_seq += rq->write.len;
	
	send_tcp_pkt(net, b, c);

	rp.write.type = NET_write_rsp;
	rp.write.ret = OK;
	rp.write.chan_id = b->id;
	rp.write.proto.tcp.con_id = c->id;
	rp.write.len = rq->write.len;
	rp.write.pa = rq->write.pa;
	rp.write.pa_len = rq->write.pa_len;

	send(b->proc_id, &rp);
}

void
ip_read_tcp(struct net_dev *net,
		union net_req *rq, 
		struct binding *b)
{
	size_t len, o, l, cont_ack;
	struct tcp_con *c;
	struct tcp_pkt *p;
	union net_rsp rp;
	uint8_t *va;

	log(LOG_INFO, "TCP read");

	c = find_tcp_con(b, rq->read.proto.tcp.con_id);
	if (c == nil) {
		log(LOG_WARNING, "couldn't find con %i", rq->read.proto.tcp.con_id);
		return;
	}

	if (c->state != TCP_state_established 
		&& !(c->state == TCP_state_close_wait && c->recv_wait != nil)) 
	{
		log(LOG_WARNING, "bad state %i", c->state);

		give_addr(b->proc_id, rq->read.pa, rq->read.pa_len);

		rp.read.type = NET_read_rsp;
		rp.read.ret = ERR;
		rp.read.chan_id = b->id;
		rp.read.proto.tcp.con_id = c->id;
		rp.read.len = 0;
		rp.read.pa = rq->read.pa;
		rp.read.pa_len = rq->read.pa_len;

		send(b->proc_id, &rp);

		return;
	}

	va = map_addr(rq->read.pa, rq->read.pa_len, MAP_RW);
	if (va == nil) {
		log(LOG_WARNING, "map failed");
		return;
	}

	len = 0;
	cont_ack = c->ack;
	while (c->recv_wait != nil && len < rq->read.len) {
		p = c->recv_wait;
		if (p->seq != cont_ack) {
			break;
		}

		o = c->offset_into_waiting;
		l = rq->read.len - len;
		if (l > p->len - o) {
			l = p->len - o;
		}

		memcpy(va + len,
			p->data + o,
			l);

		if (o + l < p->len) {
			c->offset_into_waiting = o + l;
		} else {
			c->offset_into_waiting = 0;
			c->recv_wait = p->next;
			free(p->data);
			free(p);
			cont_ack += l;
		}

		len += l;
	}

	if (c->ack < cont_ack) {
		log(LOG_INFO, "increase ack from 0x%x to 0x%x", c->ack, cont_ack);

		c->ack = cont_ack;
		add_pkt(c, TCP_flag_ack, nil, 0);
		send_tcp_pkt(net, b, c);
	}

	if (c->state == TCP_state_close_wait && c->recv_wait == nil) {	
		log(LOG_INFO, "ack fin now that everything has been read");

		c->ack++;
		add_pkt(c, TCP_flag_ack, nil, 0);

		send_tcp_pkt(net, b, c);
	}

	unmap_addr(va, rq->read.pa_len);

	give_addr(b->proc_id, rq->read.pa, rq->read.pa_len);

	rp.read.type = NET_read_rsp;
	rp.read.ret = OK;
	rp.read.chan_id = b->id;
	rp.read.proto.tcp.con_id = c->id;
	rp.read.len = len;
	rp.read.pa = rq->read.pa;
	rp.read.pa_len = rq->read.pa_len;

	send(b->proc_id, &rp);
}
