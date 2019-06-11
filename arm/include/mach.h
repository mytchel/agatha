#define MAX_PROCS      16
#define MAX_MESSAGES   128

#define KSTACK_LEN       1024

typedef struct label label_t;

struct label {
  uint32_t psr, sp, lr;
  uint32_t regs[13];
  uint32_t pc;
} __attribute__((__packed__));

struct intr_mapping {
	int pid;
	int irqn;
	void *func;
	void *arg;
	void *sp;
};

struct kernel_info {
	size_t boot_pa, boot_len;
	size_t kernel_pa, kernel_len;
	size_t proc0_pa, proc0_len;
	size_t bundle_pa, bundle_len;
	size_t info_pa, info_len;

	size_t kernel_va;	

	struct {	
		size_t l1_va, l2_va;
		size_t l1_pa, l1_len;
		size_t l2_pa, l2_len;
	} kernel;

	struct {
		size_t l1_va, l2_va;
		size_t l1_pa, l1_len;
		size_t l2_pa, l2_len;

		size_t info_va;
		size_t stack_pa, stack_va, stack_len;
	} proc0;
};

