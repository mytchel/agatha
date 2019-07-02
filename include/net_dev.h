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

struct arp_entry {
	uint8_t ip[4];
	uint8_t mac[6];
};

struct net_dev {
	void *arg;

	uint16_t ipv4_ident;

	size_t mtu;

	uint8_t mac[6];
	uint8_t ipv4[4];

	void (*send_pkt)(struct net_dev *dev,
			uint8_t *buf, size_t len);

	struct arp_entry *arp_entries;
	struct ip_port *ip_ports;
};

void
net_process_pkt(struct net_dev *dev,
		uint8_t *buf, size_t len);

void
net_handle_message(struct net_dev *dev,
		int from, uint8_t *m);

int
net_init(struct net_dev *dev);

