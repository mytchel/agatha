union timer_req {
	uint8_t raw[MESSAGE_LEN];
	uint32_t type;

	struct {
		uint32_t type;
		uint32_t time_ms;
	} set;

	struct {
		uint32_t type;
	} triggered;
};

union timer_rsp {
	uint8_t raw[MESSAGE_LEN];

	struct {
		uint32_t type;
		int ret;
	} untyped;

	struct {
		uint32_t type;
		int ret;
	} set;

	struct {
		uint32_t type;
		int ret;
	} triggered;
};

