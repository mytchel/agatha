struct binding {
	struct binding *next;

	int proc_id;
	int id;

 	uint8_t proto;
	void *proto_arg;
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

	size_t n_binding_id;
	size_t n_free_port;
	struct binding *bindings;

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

int
ip_bind(struct net_dev *net,
		union net_req *rq, 
		struct binding *c);

int
ip_unbind(struct net_dev *net,
		union net_req *rq, 
		struct binding *c);

void
ip_write_udp(struct net_dev *net,
		union net_req *rq, int cap,
		struct binding *c);

void
ip_read_udp(struct net_dev *net,
		union net_req *rq, int cap, 
		struct binding *c);

void
ip_tcp_listen(struct net_dev *net,
		union net_req *rq, 
		struct binding *c);

void
ip_tcp_connect(struct net_dev *net,
		union net_req *rq, 
		struct binding *c);

void
ip_tcp_disconnect(struct net_dev *net,
		union net_req *rq, 
		struct binding *c);

void
ip_read_tcp(struct net_dev *net,
		union net_req *rq, int cap,
		struct binding *c);

void
ip_write_tcp(struct net_dev *net,
		union net_req *rq, int cap,
		struct binding *c);

