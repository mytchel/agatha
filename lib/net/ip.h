struct ip_pkt_frag {
	size_t offset;
	uint8_t *data;
	size_t len;

	struct ip_pkt_frag *next;
};

struct ip_pkt {
	struct ip_pkt *next;

	uint8_t src_ipv4[4];
	uint8_t dst_ipv4[4];
	uint8_t protocol;
	uint16_t id;

	uint16_t src_port;

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

struct tcp_con {
	struct tcp_con *next;

	struct binding *binding;

	int id;

	uint16_t port_rem;
	uint8_t mac_rem[6];
	uint8_t addr_rem[4];

	tcp_state state;
	bool closing;
		
	size_t window_size_loc;
	size_t window_size_rem;
	size_t window_sent;

	uint32_t ack;

	uint32_t next_seq;
	struct tcp_pkt *sending;

	size_t offset_into_waiting;
	struct tcp_pkt *recv_wait;
};

struct binding_ip {
	uint8_t addr_loc[4];
	uint16_t port_loc;

	struct {
		struct tcp_con *listen_con;
		int next_con_id;
		struct tcp_con *cons;
	} tcp;

	struct {
		size_t offset_into_waiting;
		struct ip_pkt *recv_wait;
	} udp;
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

struct binding *
find_binding_ip(struct net_dev *net,
	int proto, uint16_t port_loc);

struct ip_pkt *
ip_pkt_new(uint8_t src[4], uint8_t dst[4],
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



