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

uint32_t
ttb[4096]__attribute__((__aligned__(16*1024))) = { L1_FAULT };

vspace_t
vspace_init(size_t pa, size_t va)
{
	vspace_t s = (vspace_t) va;
	size_t l1_off, l2_off;
	int i;
	
	l1_off = (size_t) s->l1 - (size_t) s;
	l2_off = (size_t) s->l2 - (size_t) s;
	
	debug("vspace init at 0x%h, l1_off = 0x%h, l2_off = 0x%h\n", 
		pa, l1_off, l2_off);
	
	for (i = 0; i < 4096; i++) {
		s->l1[i] = L1_FAULT;
	}
	
	for (i = SECTION_ALIGN_DN(&_kernel_start)/SECTION_SIZE;
	     i < SECTION_ALIGN(&_kernel_end)/SECTION_SIZE; 
	     i++) {
		s->l1[i] = ttb[i];
	}
		
	for (i = 0; i < 256; i++) {
		s->l2[i] = L2_FAULT;
	}
	
	map_l2(s->l1, pa + l2_off, KERNEL_VA - SECTION_SIZE);
	map_pages(s->l2, pa, 
	          KERNEL_VA - SECTION_SIZE, 
	          sizeof(struct vspace), 
	          AP_RW_RO, true);
	          
	return (vspace_t) pa;
}

int
mmu_switch(vspace_t s)
{
	debug("switching to vspace 0x%h\n", s);
	
	mmu_load_ttb((uint32_t *) s);
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
	
	for (o = 0; o < f->u.len; o += PAGE_SIZE) {
		map_l2(l1, f->u.pa + o, va + o);
	}
	
	f->u.va = va;
	f->u.flags = F_MAP_TYPE_TABLE;
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

/* TODO: need another data field to store m_addr 
   as well as where it is in the page tables. */
int
frame_table(void *tb, kframe_t f, 
            size_t m_addr, int type)
{
	int r;
	
	r = frame_map(tb, f, m_addr, 
	              F_MAP_TYPE_PAGE|F_MAP_READ);
	if (r != OK) {
		return r;
	}
	
	memset((void *) m_addr, f->u.len, 0);
	
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
		
	case F_MAP_TYPE_TABLE:
		return frame_map_l2(tb, f, va);
	}
}
