int
map_l2(uint32_t *l1, size_t pa, size_t va);

#define AP_NO_NO	0
#define AP_RW_NO	1
#define AP_RW_RO	2
#define AP_RW_RW	3

int
map_pages(uint32_t *l2, size_t pa, size_t va, 
          size_t len, int ap, bool cache);

int
unmap_pages(uint32_t *l2, size_t va, size_t len);

int
map_sections(uint32_t *l1, size_t pa, size_t va, 
             size_t len, int ap, bool cache);

int
unmap_sections(uint32_t *l1, size_t va, size_t len);

