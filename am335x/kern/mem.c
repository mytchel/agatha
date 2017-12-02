/*
 *
 * Copyright (c) 2017 Mytchel Hammond <mytch@lackname.org>
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

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
mmu_switch(void *v)
{
	debug("load ttb 0x%h\n", v);
	mmu_load_ttb((uint32_t *) v);

	debug("invalidate\n");
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
frame_map_l2(uint32_t *l1, kframe_t f, size_t va)
{
	size_t o;

	/* Must already be mapped somewhere as read only. */
	if (f->u.flags & F_MAP_WRITE) {
		return ERR;
	} else if (!(f->u.flags & F_MAP_READ)) {
		return ERR;
	} else {
		memset((void *) f->u.va, 0, f->u.len);
	}

	for (o = 0; o < f->u.len; o += PAGE_SIZE) {
		map_l2(l1, f->u.pa + o, va + o);
	}
	
	f->u.t_va = va;
	f->u.flags = F_MAP_TYPE_TABLE_L2|F_MAP_READ;

	return OK;
}

static int
frame_map_sections(uint32_t *l1, kframe_t f, size_t va, int flags)
{
	uint32_t ap;
	
	if (flags & F_MAP_WRITE) {
		ap = AP_RW_RW;
	} else {
		ap = AP_RW_RO;
	}
	
	map_sections(l1, f->u.pa, va, f->u.len, ap, false);
	
	f->u.va = va;
	f->u.flags = flags;
	
	return OK;
}

static int
frame_map_pages(uint32_t *l2, kframe_t f, size_t va, int flags)
{
	uint32_t ap;
	int r;
		
	if (flags & F_MAP_WRITE) {
		ap = AP_RW_RW;
	} else {
		ap = AP_RW_RO;
	}
	
	r = map_pages(l2, f->u.pa, va, f->u.len, ap, false);
	if (r != OK) {
		return r;
	}
	
	f->u.va = va;
	f->u.flags = flags;
	
	return OK;
}

int
frame_map(void *tb, kframe_t f, size_t va, int flags)
{
	int r;
	
	debug("frame map to table 0x%h from 0x%h to 0x%h with flags %i\n",
		tb, f->u.pa, va, flags);

	switch (flags & F_MAP_TYPE_MASK) {
	default:
	case F_MAP_TYPE_PAGE:
		r = frame_map_pages(tb, f, va, flags);
		return r;
		
	case F_MAP_TYPE_SECTION:
		r = frame_map_sections(tb, f, va, flags);
		return r;
	
	case F_MAP_TYPE_TABLE_L1:
		return ERR;
	
	case F_MAP_TYPE_TABLE_L2:
		return frame_map_l2(tb, f, va);
	}
}

