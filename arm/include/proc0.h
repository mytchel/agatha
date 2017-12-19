#define proc0_type_dev_compatability     1

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
  size_t nintrs;
  int intrs[4];
  size_t nframes;
  int frames[4];
};

