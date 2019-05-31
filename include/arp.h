struct arp_pkt {
	uint8_t h_type[2];
	uint8_t p_type[2];
	uint8_t h_len;
	uint8_t p_len;
	uint8_t oper[2];
	uint8_t src[6];
	uint8_t src_prot[4];
	uint8_t dst[6];
	uint8_t dst_prot[4];
};

