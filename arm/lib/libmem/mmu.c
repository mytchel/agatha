#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <arm/mmu.h>

	int
map_l2(uint32_t *l1, size_t pa, size_t va, size_t len)
{
	size_t o;

	/* TODO: is this right? */	
	for (o = 0; (o << PAGE_SHIFT) < len; o++) {
		l1[L1X(va + (o << SECTION_SHIFT))] = 
			(pa + (o << PAGE_SHIFT)) | L1_COARSE;
	}

	return OK;
}

	int
unmap_l2(uint32_t *l1, size_t va, size_t len)
{
	size_t o;
	
	for (o = 0; (o << PAGE_SHIFT) < len; o++) {
		l1[L1X(va + (o << SECTION_SHIFT))] = 0;
	}

	return OK;
}

	int
map_pages(uint32_t *l2, size_t pa, size_t va, 
		size_t len, int ap, bool cache)
{
	uint32_t tex, c, b, o;

	if (cache) {
		tex = 7;
		c = 1;
		b = 0;
	} else {
		tex = 0;
		c = 0;
		b = 1;
	}

	for (o = 0; o < len; o += PAGE_SIZE) {
		l2[L2X(va + o)] = (pa + o) | L2_SMALL | 
			tex << 6 | ap << 4 | c << 3 | b << 2;
	}

	return OK;
}

	int
unmap_pages(uint32_t *l2, size_t va, size_t len)
{
	uint32_t o;

	for (o = 0; o < len; o += PAGE_SIZE) {
		l2[L2X(va + o)] = L2_FAULT;
	}

	return OK;
}

	int
map_sections(uint32_t *l1, size_t pa, size_t va, 
		size_t len, int ap, bool cache)
{
	uint32_t tex, c, b, o;

	if (cache) {
		tex = 7;
		c = 1;
		b = 0;
	} else {
		tex = 0;
		c = 0;
		b = 1;
	}

	for (o = 0; o < len; o += SECTION_SIZE) {
		l1[L1X(va + o)] = (pa + o) | L1_SECTION | 
			tex << 12 | ap << 10 | c << 3 | b << 2;
	}

	return OK;
}

	int
unmap_sections(uint32_t *l1, size_t va, size_t len)
{
	uint32_t o;

	for (o = 0; o < len; o += SECTION_SIZE) {
		l1[L1X(va + o)] = L1_FAULT;
	}

	return OK;
}

