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

bool
create_eth_pkt(struct net_dev *net,
		uint8_t *dst_mac, 
		int16_t type,
		size_t len,
		uint8_t **pkt,
		uint8_t **body)
{
	struct eth_hdr *eth_hdr;
	size_t pkt_len;

	pkt_len = sizeof(struct eth_hdr) + len;

	*pkt = malloc(sizeof(struct eth_hdr) + pkt_len);
	if (*pkt == nil) {
		log(LOG_WARNING, "malloc failed for pkt");
		return false;
	}

	eth_hdr = (void *) *pkt;

	memcpy(eth_hdr->src, net->mac, 6);
	memcpy(eth_hdr->dst, dst_mac, 6);
	
	eth_hdr->tol.type[0] = (type >> 8) & 0xff;
	eth_hdr->tol.type[1] = (type >> 0) & 0xff;

	*body = *pkt + sizeof(struct eth_hdr);

	return true;
}

	void
send_eth_pkt(struct net_dev *net,
		uint8_t *pkt,
		size_t len)
{
	size_t pkt_len;

	pkt_len = sizeof(struct eth_hdr) + len;
	
	net->send_pkt(net, pkt, pkt_len);
}

