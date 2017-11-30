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

uint32_t
ttb[4096]__attribute__((__aligned__(16*1024))) = { L1_FAULT };

int
mmu_switch(proc_t p)
{
	uint32_t *t;
	int i;
	
	if (p->base == nil) {
		return ERR;
	}
	
	t = (uint32_t *) p->base->u.pa;
	
	/* Limit virtual space for now. */
	for (i = 0; i < 1000; i++) {
		ttb[i] = t[i];
	}
	
	mmu_invalidate();
	
	return OK;	
}

int
map_l2(uint32_t *ttb, size_t pa, size_t va)
{
	uint32_t *l2;
	int i;
	
	debug("map l2 0x%h -> 0x%h\n", pa, va);
	
	ttb[L1X(va)] = pa | L1_COARSE;
	
	l2 = (uint32_t *) (pa);
	for (i = 0; i < 256; i++) {
		l2[i] = L2_FAULT;
	}
	
	return OK;
}

int
map_pages(uint32_t *ttb, size_t pa, size_t va, 
          size_t len, int ap, bool cache)
{
	uint32_t *l2, tex, c, b, o;
	
	tex = 0;
	c = 0;
	b = 1;
	
	for (o = 0; o < len; o += PAGE_SIZE) {
		debug("page 0x%h -> 0x%h\n", pa + o, va + o);
		
		/* TODO: is this right? */
		l2 = (uint32_t *) (ttb[L1X(va + o)] & ~((1 << 10) - 1));
		
		if (l2 == nil) {
			/* TODO: unmap what was mapped. */
			return ERR;
		}
				
		l2[L2X(va + o)] = (pa + o) | L2_SMALL | 
		    tex << 6 | ap << 4 | c << 3 | b << 2;
	}
	
	return OK;
}

int
map_sections(uint32_t *ttb, size_t pa, size_t va, 
             size_t len, int ap, bool cache)
{
	uint32_t tex, c, b, o;
	
	tex = 0;
	c = 0;
	b = 1;
	
	for (o = 0; o < len; o += SECTION_SIZE) {
		debug("section 0x%h -> 0x%h\n", pa + o, va + o);
		
		ttb[L1X(va + o)] = (pa + o) | L1_SECTION | 
		    tex << 12 | ap << 10 | c << 3 | b << 2;
	}
	
	return OK;
}

static int
frame_map_base(proc_t p, kframe_t f, size_t va)
{
	uint32_t *ttb;
	int i;
	
	if (va != 0 || f->u.len != PAGE_SIZE * 4) {
		return ERR;
	}
	
	p->base = f;
	
	ttb = (uint32_t *) f->u.pa;
	
	for (i = 0; i < 4096; i++) {
		ttb[i] = L1_FAULT;
	}
	
	f->u.va = va;
	f->u.flags = F_MAP_L1_TABLE;
	
	return OK;
}

static int
frame_map_l2(uint32_t *ttb, kframe_t f, size_t va)
{
	size_t o;
	
	for (o = 0; o < f->u.len; o += PAGE_SIZE) {
		map_l2(ttb, f->u.pa + o, va + o);
	}
	
	f->u.va = va;
	f->u.flags = F_MAP_L2_TABLE;
	return OK;
}

static int
frame_map_sections(uint32_t *ttb, kframe_t f, size_t va, int flags)
{
	uint32_t ap;
	
	if (flags & F_MAP_WRITE) {
		ap = AP_RW_RW;
	} else {
		ap = AP_RW_RO;
	}
	
	map_sections(ttb, f->u.pa, va, f->u.len, ap, false);
	
	f->u.va = va;
	f->u.flags = flags;
	
	return OK;
}

static int
frame_map_pages(uint32_t *ttb, kframe_t f, size_t va, int flags)
{
	uint32_t ap;
	int r;
		
	if (flags & F_MAP_WRITE) {
		ap = AP_RW_RW;
	} else {
		ap = AP_RW_RO;
	}
	
	r = map_pages(ttb, f->u.pa, va, f->u.len, ap, false);
	if (r != OK) {
		return r;
	}
	
	f->u.va = va;
	f->u.flags = flags;
	
	return OK;
}

int
frame_map(proc_t p, kframe_t f, size_t va, int flags)
{
	uint32_t *ttb;
	
	debug("frame_map for %i from 0x%h to 0x%h with %i\n",
		    p->pid, f->u.pa, va, flags);
	
	if (va >= 1000 * SECTION_SIZE) {
		debug("No mappings past 0x%h\n", 1000 * SECTION_SIZE);
		return ERR;
	}
	
	if (flags & F_MAP_L1_TABLE) {
		debug("mapping base\n");
		
		return frame_map_base(p, f, va);
				
	} else if (p->base == nil) {
		debug("no base\n");
		
		return ERR;
		
	}
	
	ttb = (uint32_t *) p->base->u.pa;
	
	if (flags & F_MAP_L2_TABLE) {
		debug("mapping l2\n");
		
		return frame_map_l2(ttb, f, va);
		
	} else if ((va & SECTION_MASK) == va && 
	            (f->u.len & SECTION_MASK) == f->u.len) {
		debug("mapping sections\n");
		
		return frame_map_sections(ttb, f, va, flags);
	
	} else {
		debug("mapping small pages\n");
		return frame_map_pages(ttb, f, va, flags);		
	}
}

int
frame_unmap(proc_t p, kframe_t f)
{
	/* TODO. */
	return ERR;
}

