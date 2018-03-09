#define SYSCALL_YIELD             0

#define SYSCALL_SEND              1
#define SYSCALL_RECV              2

#define SYSCALL_FRAME_SPLIT       3
#define SYSCALL_FRAME_MERGE       4
#define SYSCALL_FRAME_GIVE        5
#define SYSCALL_FRAME_MAP         6
#define SYSCALL_FRAME_TABLE       7
#define SYSCALL_FRAME_UNMAP       8
#define SYSCALL_FRAME_ALLOW       9
#define SYSCALL_FRAME_COUNT      10
#define SYSCALL_FRAME_INFO       11
#define SYSCALL_FRAME_INFO_INDEX 12

/* Proc0 only syscalls */
#define SYSCALL_PROC_NEW         13
#define SYSCALL_FRAME_CREATE     14

#define NSYSCALLS                15

#define MESSAGE_LEN 64
