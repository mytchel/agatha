#define MAP_RO         (0<<0)
#define MAP_RW         (1<<0)

#define MAP_MEM        (0<<1)
#define MAP_DEV        (1<<1) 
#define MAP_SHARED     (2<<1) 
#define MAP_TABLE      (3<<1) 
#define MAP_TYPE_MASK  (3<<1) 

#define MAX_PROCS      16
#define MAX_MESSAGES   32
#define KSTACK_LEN   1024

typedef struct label label_t;

struct label {
  uint32_t psr, sp, lr;
  uint32_t regs[13];
  uint32_t pc;
} __attribute__((__packed__));

struct kernel_info {
	size_t kernel_pa, kernel_len;
	size_t bundle_pa, bundle_len;

	size_t info_pa, info_len;
	
	size_t dtb_pa, dtb_len;

	struct {
		uint32_t *l1_va, *l2_va;
		size_t l1_pa, l1_len;
		size_t l2_pa, l2_len;
		
		size_t stack_pa, stack_va, stack_len;
		size_t prog_pa, prog_va, prog_len;
		size_t info_va;
	} proc0;
};

