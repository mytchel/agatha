#define LOG_PID 2
#define LOG_SERVICE_NAME_LEN 32

enum {
	LOG_FATAL,
	LOG_WARNING,
	LOG_INFO,
	LOG_MAX_LEVEL,
};

union log_req {
	uint8_t raw[MESSAGE_LEN];
	uint32_t type;

	struct {
		uint32_t type;
		char name[LOG_SERVICE_NAME_LEN];
		bool respond;
	} reg;

	struct {
		uint32_t type;
		uint8_t level;
	 	char mesg[MESSAGE_LEN - sizeof(uint32_t) - sizeof(uint8_t)];	
	} log __attribute__((packed));
};

union log_rsp {
	uint8_t raw[MESSAGE_LEN];
	uint32_t type;

	struct {
		uint32_t type;
		int ret;
	} reg;

	struct {
		uint32_t type;
		int ret;
	} log;
};

int
log_init(char *name);

void
log(int level, char *fmt, ...);

