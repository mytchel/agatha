#include <head.h>
#include "fns.h"

#define L1X(va)          ((va) >> 20)
#define L2X(va)          (((va) >> 12) & ((1 << 8) - 1))

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

extern uint32_t *_kernel_start;
extern uint32_t *_kernel_end;

	int
vspace_create(void *tb, kframe_t f)
{
	if (f->u.len != 0x4000) {
		return ERR;
	} else if ((f->u.flags & F_MAP_WRITE)) {
		return ERR;
	} else if (!(f->u.flags & F_MAP_READ)) {
		return ERR;
	} else {
		memcpy((void *) f->u.va, ttb, 0x4000);

		f->u.flags |= F_MAP_TYPE_TABLE_L1;

		return OK;
	}
}

	int
vspace_swap(proc_t from, proc_t to, kframe_t f)
{
	kframe_t n, nn;

	if (!(f->u.flags & F_MAP_TYPE_TABLE_L1)) {
		return ERR;
	} else {
		n = from->frames;
		while (n != nil) {
			nn = n->next;

			if (n->u.v_id == f->u.f_id) {
				frame_remove(from, n);
				frame_add(to, n);
			}

			n = nn;
		}

		to->vspace = (void *) f->u.pa;

		return OK;
	}
}

	int
mmu_switch(kframe_t vs)
{
	debug("switching to vspace 0x%h\n", vs->u.pa);

	mmu_load_ttb((uint32_t *) vs->u.pa);
	mmu_invalidate();

	debug("ok\n");

	return OK;
}

	int
map_l2(uint32_t *l1, size_t pa, size_t va)
{
	debug("map l2 0x%h -> 0x%h\n", pa, va);

	l1[L1X(va)] = pa | L1_COARSE;

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
		debug("page 0x%h -> 0x%h\n", pa + o, va + o);

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
		debug("unmap page 0x%h\n", va + o);	

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
		debug("unmap section 0x%h\n", va + o);

		l1[L1X(va + o)] = L1_FAULT;
	}

	return OK;
}

	static int
frame_map_l2(kframe_t v, uint32_t *l1, kframe_t f, size_t va)
{
	size_t o;

	if (f->u.flags & F_MAP_WRITE) {
		return ERR;
	} else if (!(f->u.flags & F_MAP_READ)) {
		return ERR;
	}

	for (o = 0; o < f->u.len; o += PAGE_SIZE) {
		map_l2(l1, f->u.pa + o, va + o);
	}

	f->u.v_id = v->u.f_id;
	f->u.t_va = va;
	f->u.flags |= F_MAP_TYPE_TABLE_L2;
	
	return OK;
}

	static int
frame_map_sections(kframe_t v, uint32_t *l1, kframe_t f, size_t va, int flags)
{
	uint32_t ap;
	int fl, r;

	fl = F_MAP_TYPE_SECTION | F_MAP_READ;
	if (flags & F_MAP_WRITE) {
		fl |= F_MAP_WRITE;
		ap = AP_RW_RW;
	} else {
		ap = AP_RW_RO;
	}

	r = map_sections(l1, f->u.pa, va, f->u.len, ap, false);
	if (r != OK) {
		return r;
	}

	f->u.v_id = v->u.f_id;
	f->u.va = va;
	f->u.flags |= fl;

	return OK;
}

static int
frame_map_pages(kframe_t v, uint32_t *l2, kframe_t f, size_t va, int flags)
{
	uint32_t ap;
	int fl, r;

	fl = F_MAP_TYPE_PAGE | F_MAP_READ;
	if (flags & F_MAP_WRITE) {
		fl |= F_MAP_WRITE;
		ap = AP_RW_RW;
	} else {
		ap = AP_RW_RO;
	}

	r = map_pages(l2, f->u.pa, va, f->u.len, ap, false);
	if (r != OK) {
		return r;
	}

	f->u.v_id = v->u.f_id;
	f->u.va = va;
	f->u.flags |= fl;

	return OK;
}

/* Should l2 be given to frame_map or l1? And l2 found from l1?
 * That would be slower but would ensure consistancy.
 * I might change it to that later on. 
 *
 * Otherwise need to check if the given v matches tb for both
 * l1 and l2 cases. */
	
	int
frame_map(kframe_t v, void *tb, kframe_t f, size_t va, int flags)
{
	debug("frame map to table 0x%h from 0x%h to 0x%h with flags %i\n",
			tb, f->u.pa, va, flags);

	switch (flags & F_MAP_TYPE_MASK) {
		default:
		case F_MAP_TYPE_PAGE:
			return frame_map_pages(v, tb, f, va, flags);

		case F_MAP_TYPE_SECTION:
			return frame_map_sections(v, tb, f, va, flags);

		case F_MAP_TYPE_TABLE_L2:
			return frame_map_l2(v, tb, f, va);

		case F_MAP_TYPE_TABLE_L1:
			return ERR;
	}
}
