#define L1X(va)          (((uint32_t) va) >> 20)
#define L2X(va)          ((((uint32_t) va) >> 12) & ((1 << 8) - 1))

#define L1VA(x)          (((uint32_t) x) << 20)
#define L2VA(x)          (((uint32_t) x) << 12)

/* These are probably wrong but should work as long
	 as nothing else here or in any mmu code changes */
#define L1PA(r)          (((uint32_t) r) & (~3))
#define L2PA(r)          (((uint32_t) r) & (~0x1ff))

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

#define TABLE_SHIFT    22
#define TABLE_AREA	 (1UL << TABLE_SHIFT)
#define TABLE_SIZE   PAGE_SIZE
#define TABLE_MASK	 (~(TABLE_AREA - 1))

#define TABLE_ALIGN(x)    ((((size_t) x) + TABLE_AREA - 1) & TABLE_MASK)
#define TABLE_ALIGN_DN(x) ((((size_t) x)) & TABLE_MASK)

#define SECTION_SHIFT  20
#define SECTION_SIZE (1UL << SECTION_SHIFT)
#define SECTION_MASK (~(SECTION_SIZE - 1))

#define SECTION_ALIGN(x)    ((((size_t) x) + SECTION_SIZE - 1) & SECTION_MASK)
#define SECTION_ALIGN_DN(x) ((((size_t) x)) & SECTION_MASK)

#define AP_NO_NO	0
#define AP_RW_NO	1
#define AP_RW_RO	2
#define AP_RW_RW	3

int
map_l2(uint32_t *l1, size_t pa, size_t va, size_t n);

int
unmap_l2(uint32_t *l1, size_t va, size_t n);

int
map_pages(uint32_t *l2, 
		size_t pa, size_t va, 
		size_t n, 
		int ap, bool cache);

int
unmap_pages(uint32_t *l2, size_t va, size_t n);

int
map_sections(uint32_t *l1, 
		size_t pa, size_t va, 
		size_t n, 
		int ap, bool cache);

int
unmap_sections(uint32_t *l1, size_t va, size_t n);

