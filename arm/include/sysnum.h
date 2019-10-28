#define SYSCALL_YIELD             0
#define SYSCALL_PID               1
#define SYSCALL_EXIT              2

#define SYSCALL_OBJ_CREATE        3
#define SYSCALL_OBJ_RETYPE        4
#define SYSCALL_OBJ_SPLIT         5
#define SYSCALL_OBJ_MERGE         6

#define SYSCALL_ENDPOINT_CONNECT  7
#define SYSCALL_MESG              8
#define SYSCALL_RECV              9
#define SYSCALL_REPLY            10
#define SYSCALL_SIGNAL           11

#define SYSCALL_PROC_SETUP       12
#define SYSCALL_PROC_SET_PRIORITY 24
#define SYSCALL_PROC_START       13

#define SYSCALL_INTR_INIT        14
#define SYSCALL_INTR_CONNECT     15
#define SYSCALL_INTR_ACK         16

#define SYSCALL_FRAME_SETUP      16
#define SYSCALL_FRAME_INFO       17
#define SYSCALL_FRAME_L1_SETUP   18
#define SYSCALL_FRAME_L2_MAP     19
#define SYSCALL_FRAME_L2_UNMAP   20
#define SYSCALL_FRAME_MAP        21
#define SYSCALL_FRAME_UNMAP      22

#define SYSCALL_DEBUG            23

#define NSYSCALLS                25

