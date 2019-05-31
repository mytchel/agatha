struct eth_hdr {
	uint8_t dst[6];
	uint8_t src[6];
	/* optional */
	/*uint8_t tag[4]; */
	union {
		uint8_t type[2]; /* ethernet II */
		uint8_t len[2];  /* ieee 802.3 */
	} tol;
};

struct eth_foot {
	uint8_t csum[4];
};

