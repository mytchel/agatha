struct ip_pkt_frag {
	size_t offset;
	uint8_t *data;
	size_t len;

	struct ip_pkt_frag *next;
};

struct ip_pkt {
	uint8_t ip_src[4], ip_dst[4];
	uint16_t port_src, port_dst;
	uint16_t id;
	struct ip_pkt_frag *frags;	

	struct ip_pkt *next;
};

struct ip_port {
	int handler_pid;
	uint16_t port;

	struct ip_pkt *waiting_pkts;

	struct ip_port *next;
};

struct arp_request {
	uint8_t ip[4];
	void (*func)(struct net_dev *net, void *arg, uint8_t *mac);
	void *arg;

	struct arp_request *next;
};

struct arp_entry {
	uint8_t ip[4];
	uint8_t mac[6];
	struct arp_entry *next;
};

struct net_dev_internal {
	struct arp_entry *arp_entries;
	struct arp_request *arp_requests;

	struct ip_port *ip_ports;
};

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

bool
arp_match_ip(struct net_dev *net,
		uint8_t *ip_src, uint8_t *mac_dst);

	void
arp_request(struct net_dev *net, uint8_t *ipv4, 
		void (*func)(struct net_dev *net, void *arg, uint8_t *mac),
		void *arg);

