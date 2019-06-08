void
print_mac(char *s, uint8_t *mac);

void
dump_hex_block(uint8_t *buf, size_t len);

bool
create_eth_pkt(struct net_dev *net,
		uint8_t *dst_mac, 
		int16_t type,
		size_t len,
		uint8_t **pkt,
		uint8_t **body);

	void
send_eth_pkt(struct net_dev *net,
		uint8_t *pkt,
		size_t len);

	void
handle_ipv4(struct net_dev *net, 
		struct eth_hdr *eth_hdr, 
		uint8_t *bdy, size_t len);

void
handle_arp(struct net_dev *net, 
		struct eth_hdr *hdr, 
		uint8_t *bdy, size_t len);

