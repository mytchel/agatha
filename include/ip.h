struct ipv4_hdr {
	uint8_t ver_len;
	uint8_t dscp_ecn;
	uint8_t length[2];
	uint8_t ident[2];
	uint8_t fragment[2];
	uint8_t ttl;

#define IP_ICMP  0x01
#define IP_TCP   0x06
#define IP_UDP   0x11
	uint8_t protocol;
	uint8_t hdr_csum[2];
	uint8_t src[4];
	uint8_t dst[4];
};

struct icmp_hdr {
	uint8_t type;
	uint8_t code;
	uint8_t csum[2];
	uint8_t rst[4];
};

struct udp_hdr {
	uint8_t port_src[2];
	uint8_t port_dst[2];
	uint8_t length[2];
	uint8_t csum[2];
};

struct tcp_hdr {
	uint8_t port_src[2];
	uint8_t port_dst[2];
	uint8_t seq[4];
	uint8_t ack[4];

#define TCP_flag_fin (1<<0)
#define TCP_flag_syn (1<<1)
#define TCP_flag_rst (1<<2)
#define TCP_flag_psh (1<<3)
#define TCP_flag_ack (1<<4)
#define TCP_flag_urg (1<<5)
#define TCP_flag_ece (1<<6)
#define TCP_flag_cwr (1<<7)
#define TCP_flag_ns  (1<<8)
	uint8_t flags[2];

	uint8_t win_size[2];
	uint8_t csum[2];
	uint8_t urg[2];
};

typedef enum {
 	TCP_state_listen,
 	TCP_state_opening,
	TCP_state_syn_sent,
	TCP_state_syn_received,
	TCP_state_established,
	TCP_state_fin_wait_1,
	TCP_state_fin_wait_2,
	TCP_state_close_wait,
	TCP_state_closing,
	TCP_state_last_ack,
	TCP_state_time_wait,
	TCP_state_closed,
} tcp_state;


