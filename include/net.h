union net_req {
	uint8_t raw[MESSAGE_LEN];
	uint32_t type;

	struct {
		uint32_t type;

		uint8_t addr_loc[4];
		uint16_t port_loc;

#define NET_proto_udp  0
#define NET_proto_tcp  1
		int proto;
	} bind;

	struct {
		uint32_t type;
		int chan_id;
	} unbind;

	struct {
		uint32_t type;
		int chan_id;

		uint8_t addr_rem[4];
		uint16_t port_rem;
	} tcp_connect;

	struct {
		uint32_t type;
		int chan_id;
		int con_id;
	} tcp_disconnect;

	struct {
		uint32_t type;
		int chan_id;
		size_t timeout_ms;
	} tcp_listen;

	struct {
		uint32_t type;
		int chan_id;
		size_t pa, pa_len;
		size_t len;
		size_t timeout_ms;

		union {
			struct {
				int con_id;
			} tcp;
		} proto;
	} read;

	struct {
		uint32_t type;
		int chan_id;
		size_t pa, pa_len;
		size_t len;

		union {
			struct {
				uint8_t addr_rem[4];
				uint16_t port_rem;
			} udp;
		} proto;
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
		int chan_id;
	} bind;

	struct {
		uint32_t type;
		int ret;
		int chan_id;
	} unbind;

	struct {
		uint32_t type;
		int ret;
		int chan_id;

		int con_id;
	} tcp_connect;

	struct {
		uint32_t type;
		int ret;
		int chan_id;
		int con_id;
	} tcp_disconnect;

	struct {
		uint32_t type;
		int ret;
		int chan_id;

		uint8_t addr_rem[4];
		uint16_t port_rem;
		int con_id;
	} tcp_listen;

	struct {
		uint32_t type;
		int ret;
		int chan_id;
		size_t pa, pa_len;
		size_t len;
		
		union {
			struct {
				uint8_t addr_rem[4];
				uint16_t port_rem;
			} udp;

			struct {
				int con_id;
			} tcp;
		} proto;
	} read;

	struct {
		uint32_t type;
		int ret;
		int chan_id;
		size_t pa, pa_len;
		size_t len;

		union {
			struct {
				int con_id;
			} tcp;
		} proto;
	} write;
};

