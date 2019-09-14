struct ip_pkt_frag {
	size_t offset;
	uint8_t *data;
	size_t len;

	struct ip_pkt_frag *next;
};

struct ip_pkt {
	uint8_t src_ipv4[4];
	uint8_t protocol;
	uint16_t id;

	size_t hdr_len;
	
	bool have_last;
	struct ip_pkt_frag *frags;
	
	uint8_t *data;
	size_t len;

	struct ip_pkt *next;
};

struct connection_ip {
	uint16_t port_loc;
	uint16_t port_rem;

	uint8_t mac_rem[6];
	uint8_t ip_rem[4];

	size_t offset_into_waiting;
	struct ip_pkt *waiting_pkts;
};

struct connection {
	struct connection *next;

	int proc_id;
	int id;

 	uint8_t proto;
	struct connection_ip ip;
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

	size_t n_connection_id;
	size_t n_free_port;
	struct connection *connections;

	struct {
		struct ip_pkt *pkts;
	} ip;
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

void
ip_open_udp(struct net_dev *net,
		union net_req *rq, 
		struct connection *c);

void
ip_open_tcp(struct net_dev *net,
		union net_req *rq, 
		struct connection *c);

void
ip_write_udp(struct net_dev *net,
		int from,
		union net_req *rq, 
		struct connection *c);

void
ip_write_tcp(struct net_dev *net,
		int from,
		union net_req *rq, 
		struct connection *c);

void
ip_read_udp(struct net_dev *net,
		int from,
		union net_req *rq, 
		struct connection *c);

void
ip_read_tcp(struct net_dev *net,
		int from,
		union net_req *rq, 
		struct connection *c);




