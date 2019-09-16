struct ip_pkt_frag {
	size_t offset;
	uint8_t *data;
	size_t len;

	struct ip_pkt_frag *next;
};

struct ip_pkt {
	struct ip_pkt *next;

	uint8_t src_ipv4[4];
	uint8_t protocol;
	uint16_t id;

	bool have_last;
	struct ip_pkt_frag *frags;
	
	uint8_t *data;
	size_t len;
};

struct tcp_pkt {
	struct tcp_pkt *next;

	uint8_t *data;
	size_t len;

	uint32_t seq;
	uint16_t flags;
};

struct connection_ip {
	uint16_t port_loc;
	uint16_t port_rem;

	uint8_t mac_rem[6];
	uint8_t ip_rem[4];

	struct {
		tcp_state state;
		
		size_t window_size_loc;
		size_t window_size_rem;
		size_t window_sent;

		uint32_t ack;

		uint32_t next_seq;
		struct tcp_pkt *sending;

		size_t offset_into_waiting;
		struct tcp_pkt *recv_wait;
	} tcp;

	size_t offset_into_waiting;
	struct ip_pkt *recv_wait;
};

void
ip_pkt_free(struct ip_pkt *p);

size_t
csum_ip_continue(uint8_t *h, size_t len, size_t sum);

size_t
csum_ip_finish(size_t sum);

size_t
csum_ip(uint8_t *h, size_t len);

bool
create_ipv4_pkt(struct net_dev *net,
	uint8_t *dst_mac, uint8_t *dst_ip,
	size_t hdr_len, 
	uint8_t proto,
	size_t data_len,
	uint8_t **pkt,
	struct ipv4_hdr **ipv4_hdr,
	uint8_t **body);

void
send_ipv4_pkt(struct net_dev *net,
	uint8_t *pkt,
	struct ipv4_hdr *ipv4_hdr,
	size_t hdr_len,
	size_t data_len);

struct connection *
find_connection_ip(struct net_dev *net,
	int proto,
	uint16_t port_loc, uint16_t port_rem,
	uint8_t ip_rem[4]);

struct ip_pkt *
ip_pkt_new(uint8_t *src,
		uint8_t proto,
		uint16_t ident);

void
ip_pkt_free(struct ip_pkt *p);

void
handle_icmp(struct net_dev *net,
	struct ip_pkt *p);

void
handle_udp(struct net_dev *net,
	struct ip_pkt *p);

void
handle_tcp(struct net_dev *net,
	struct ip_pkt *p);



