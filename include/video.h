union video_req {
	uint8_t raw[MESSAGE_LEN];
	uint32_t type;

	struct {
		uint32_t type;
	} connect;

	struct {
		uint32_t type;
		size_t frame_pa, frame_size;
	} update;
};

union video_rsp {
	uint8_t raw[MESSAGE_LEN];

	struct {
		uint32_t type;
		int ret;
	} untyped;

	struct {
		uint32_t type;
		int ret;
		size_t width, height;
		size_t frame_size;
	} connect;

	struct {
		uint32_t type;
		int ret;
		size_t frame_pa, frame_size;
	} update;
};

