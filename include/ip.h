struct ipv4_hdr {
	uint8_t ver_len;
	uint8_t dscp_ecn;
	uint8_t length[2];
	uint8_t ident[2];
	uint8_t fragment[2];
	uint8_t ttl;
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

