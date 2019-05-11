union proc_mesg {
	uint8_t raw[MESSAGE_LEN];
	uint32_t type;

	struct {
		uint32_t type;
	} fault;

	struct {
		uint32_t type;
		uint32_t code;
	} exit;
};


