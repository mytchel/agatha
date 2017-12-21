#define proc0_type_dev                 1

struct proc0_message {
  int type;
  uint8_t body[];
};

struct proc0_dev_req {
  int type;
#define proc0_dev_req_method_phandle    1
#define proc0_dev_req_method_compat     2
#define proc0_dev_req_method_path       3
  int method;
  union {
    uint32_t phandle;
    char compatability[56];
    char path[56];
  } kind;
};

struct proc0_dev_resp {
  int type;
  size_t nintrs;
  int intrs[4];
  size_t nframes;
  int frames[4];
};

