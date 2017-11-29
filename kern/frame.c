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

static struct kframe frames[MAX_FRAMES] = {0};
static int next_free = 0;

kframe_t
frame_new(proc_t p, size_t pa, size_t len, int type)
{
	kframe_t f;
	
	if (next_free == MAX_FRAMES) {
		return nil;
	}
	
	n = &frames[next_free++];
	
	n->u.type = type;
	n->u.pa = pa;
	n->u.len = len;
	n->u.va = 0;
	n->u.flags = 0;
	
	n->next = nil;
	
	n->u.f_id = up->frame_next_id++;
	
	for (p = up->frames; p->next != nil; p = p->next)
		;
	
	p->next = n;
	n->next = nil;
	
	return n;
}

kframe_t
frame_split(kframe_t f, size_t offset)
{
	kframe_t n, p;
	
	n = frame_new(up, f->pa + offset, f->len - offset, f->type);
		
	return n;
}

int
frame_merge(kframe_t a, kframe_t b)
{
	/* TODO. */
	return ERR;
}

