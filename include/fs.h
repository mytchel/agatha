#define FILE_ATTR_rd   1
#define FILE_ATTR_wr   2
#define FILE_ATTR_dir  4

#define FILE_root_fid  0

#define FILE_start      5 
#define FILE_find       0
#define FILE_stat       1
#define FILE_read       2
#define FILE_write      3
#define FILE_clunk      4

union file_req {
	uint8_t raw[MESSAGE_LEN];
	int type;
	
	struct {
		int type;
		int dev_pid;
		int partition;
	} start;

	struct {
		int type;
		int fid;
		char name[32];
	} find;

	struct {
		int type;
		int fid;
	} stat;	
	
	struct {
		int type;
		int fid;
		size_t pa, m_len;
		size_t offset, r_len;
	} read;	

	struct {
		int type;
		int fid;
		size_t pa, m_len;
		size_t offset, r_len;
	} write;	

	struct {
		int type;
		int fid;
	} clunk;
};

union file_rsp {
	uint8_t raw[MESSAGE_LEN];

	struct {
		int type;
		int ret;
	} untyped;
	
	struct {
		int type;
		int ret;
	} start;

	struct {
		int type;
		int ret;
		int fid;
	} find;

	struct {
		int type;
		int ret;
		size_t size, dsize;
		int attr;
	} stat;	
	
	struct {
		int type;
		int ret;
	} read;	

	struct {
		int type;
		int ret;
	} write;	

	struct {
		int type;
		int ret;
	} clunk;
};


