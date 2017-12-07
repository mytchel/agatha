#define beto16(X) \
  ((((X)>>8) & 0x00ff) | (((X)<<8) & 0xff00))

#define beto32(X) \
  (beto16((X)>>16) | \
   (beto16((X) & 0xffff) << 16))

#define beto64(X) \
  (beto32((X)>>32) | \
   (beto32((X) & 0xffffffff) << 32))


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

#define F_TYPE_IO    1
#define F_TYPE_MEM   2

/* Kind of mapping. */
#define F_MAP_TYPE_PAGE          (0<<F_MAP_TYPE_SHIFT)
#define F_MAP_TYPE_SECTION       (1<<F_MAP_TYPE_SHIFT)
#define F_MAP_TYPE_TABLE_L1      (2<<F_MAP_TYPE_SHIFT)
#define F_MAP_TYPE_TABLE_L2      (3<<F_MAP_TYPE_SHIFT)

typedef struct label label_t;

struct label {
  uint32_t psr, sp, lr;
  uint32_t regs[13];
  uint32_t pc;
} __attribute__((__packed__));

