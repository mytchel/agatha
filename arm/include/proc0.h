#define proc0_type_dev_compatability     1
#define proc0_type_dev                   2

struct proc0_message {
  int type;
  uint8_t body[];
};

struct proc0_dev_compatability_req {
  int type;
  char compatability[];
};

struct proc0_dev_compatability_resp {
  int type;
  size_t nphandles;
  uint32_t phandles[];
};

struct proc0_dev_req {
  int type;
  uint32_t phandle;
};

struct proc0_dev_resp {
  int type;
  size_t nintrs;
  int intrs[4];
  size_t nframes;
  int frames[4];
};

