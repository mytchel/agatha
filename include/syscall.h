
int
yield(void);

int
obj_create(int fid, int nid);

int
obj_retype(int cid, int type, size_t n);

int
obj_split(int cid, int nid);

int
obj_merge(int cid_l, int cid_h);

int
mesg_cap(int eid, void *rq, void *rp, int cid);

int
mesg(int eid, void *rq, void *rp);

int
recv_cap(int eid, int *pid, void *m, int cid);

int
recv(int eid, int *pid, void *m);

int
reply_cap(int eid, int pid, void *m, int cid);

int
reply(int eid, int pid, void *m);

int
signal(int eid, uint32_t s);

int
endpoint_connect(int cid, int nid);

int
intr_connect(int iid, int eid, uint32_t signal);

int
intr_ack(int iid);

int
pid(void);

void
exit(uint32_t code)
	__attribute__((noreturn));

int
proc_setup(int cid, int l1, int clist, int parent_eid);

int
proc_set_priority(int cid, size_t priority);

int
proc_start(int cid, size_t pc, size_t sp);

#include <syscall_mach.h>

