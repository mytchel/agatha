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

	csum = csum_ip((uint8_t *) icmp_hdr, 
			sizeof(struct icmp_hdr) + data_len);

	icmp_hdr->csum[0] = (csum >> 8) & 0xff;
	icmp_hdr->csum[1] = (csum >> 0) & 0xff;

	send_ipv4_pkt(net, pkt, ip_hdr, 
				20, 
				sizeof(struct icmp_hdr) +	data_len);
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

	send_icmp_pkt(net, pkt, ip_hdr_r, icmp_hdr_r, bdy_len);

	free(pkt);
}

	void
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

