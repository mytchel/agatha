#define FILE_ATTR_rd   1
#define FILE_ATTR_wr   2
#define FILE_ATTR_dir  4

#define FILE_root_fid  0

union file_req {
	uint8_t raw[MESSAGE_LEN];
	uint32_t type;
	
	struct {
		uint32_t type;
		int dev_pid;
		int partition;
	} start;

	struct {
		uint32_t type;
		int fid;
		char name[32];
	} find;

	struct {
		uint32_t type;
		int fid;
	} stat;	
	
	struct {
		uint32_t type;
		int fid;
		size_t pa, m_len;
		size_t offset, r_len;
	} read;	

	struct {
		uint32_t type;
		int fid;
		size_t pa, m_len;
		size_t offset, r_len;
	} write;	

	struct {
		uint32_t type;
		int fid;
	} clunk;
};

union file_rsp {
	uint8_t raw[MESSAGE_LEN];

	struct {
		uint32_t type;
		int ret;
	} untyped;
	
	struct {
		uint32_t type;
		int ret;
	} start;

	struct {
		uint32_t type;
		int ret;
		int fid;
	} find;

	struct {
		uint32_t type;
		int ret;
		size_t size, dsize;
		int attr;
	} stat;	
	
	struct {
		uint32_t type;
		int ret;
	} read;	

	struct {
		uint32_t type;
		int ret;
	} write;	

	struct {
		uint32_t type;
		int ret;
	} clunk;
};


