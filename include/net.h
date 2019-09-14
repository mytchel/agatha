union net_req {
	uint8_t raw[MESSAGE_LEN];
	uint32_t type;

	struct {
		uint32_t type;

		uint8_t addr[4];
		uint16_t port;

#define NET_UDP   1
#define NET_TCP   2
		uint8_t proto;
	} open;

	struct {
		uint32_t type;
		int id;
	} close;

	struct {
		uint32_t type;
		int id;
		size_t pa, len;
		size_t r_len;
		size_t timeout_ms;
	} read;

	struct {
		uint32_t type;
		int id;
		size_t pa, len;
		size_t w_len;
	} write;
};

union net_rsp {
	uint8_t raw[MESSAGE_LEN];

	struct {
		uint32_t type;
		int ret;
	} untyped;

	struct {
		uint32_t type;
		int ret;
		int id;
	} open;

	struct {
		uint32_t type;
		int ret;
		int id;
	} close;

	struct {
		uint32_t type;
		int ret;
		int id;
		size_t pa, len;
		size_t r_len;
	} read;

	struct {
		uint32_t type;
		int ret;
		int id;
		size_t pa, len;
		size_t w_len;
	} write;
};

