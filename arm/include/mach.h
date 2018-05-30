
#define beto16(X) \
  ((((X)>>8) & 0x00ff) | (((X)<<8) & 0xff00))

#define beto32(X) \
  (beto16((X)>>16) | \
   (beto16((X) & 0xffff) << 16))

#define beto64(X) \
  (beto32((X)>>32) | \
   (beto32((X) & 0xffffffff) << 32))


#define L1X(va)          ((va) >> 20)
#define L2X(va)          (((va) >> 12) & ((1 << 8) - 1))

#define L1VA(x)          ((x) << 20)
#define L2VA(x)          ((x) << 12)

#define L1_TYPE      0b11
#define L1_FAULT     0b00
#define L1_COARSE    0b01
#define L1_SECTION   0b10
#define L1_FINE      0b11

#define L2_TYPE      0b11
#define L2_FAULT     0b00
#define L2_LARGE     0b01
#define L2_SMALL     0b10
#define L2_TINY      0b11

#define PAGE_SHIFT 	 12
#define PAGE_SIZE	 (1UL << PAGE_SHIFT)
#define PAGE_MASK	 (~(PAGE_SIZE - 1))

#define PAGE_ALIGN(x)    ((((size_t) x) + PAGE_SIZE - 1) & PAGE_MASK)
#define PAGE_ALIGN_DN(x) ((((size_t) x)) & PAGE_MASK)

#define SECTION_SHIFT  20
#define SECTION_SIZE (1UL << SECTION_SHIFT)
#define SECTION_MASK (~(SECTION_SIZE - 1))

#define SECTION_ALIGN(x)    ((((size_t) x) + SECTION_SIZE - 1) & SECTION_MASK)
#define SECTION_ALIGN_DN(x) ((((size_t) x)) & SECTION_MASK)


#define MAP_RO         (0<<0)
#define MAP_RW         (1<<0)
#define MAP_MEM        (0<<1)
#define MAP_DEV        (1<<1) 
#define MAP_SHARED     (2<<1) 
#define MAP_TYPE_MASK  (3<<1) 

#define MAX_PROCS      32
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

	struct {
		uint32_t *l1_va, *l2_va;
		size_t l1_pa, l1_len;
		size_t l2_pa, l2_len;
		
		size_t stack_pa, stack_va, stack_len;
		size_t prog_pa, prog_va, prog_len;
		size_t info_va;
	} proc0;
};

