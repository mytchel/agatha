#include <head.h>

static struct kframe frames[MAX_FRAMES] = {0};
static int next_free = 0;

kframe_t
frame_new(size_t pa, size_t len, int type)
{
	kframe_t n;
	
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
	
	n->u.f_id = 0;
	
	return n;
}

void
frame_add(proc_t p, kframe_t f)
{
	kframe_t *b;
	
	p->frame_count++;
	f->u.f_id = p->frame_next_id++;
		
	f->next = nil;
	
	for (b = &p->frames; *b != nil; b = &(*b)->next)
		;
	
	*b = f;
}

void
frame_remove(proc_t p, kframe_t f)
{
	kframe_t *b;
	
	p->frame_count--;
	
	for (b = &p->frames; *b != f; b = &(*b)->next)
		;
	
	*b = f->next;
}

kframe_t
frame_split(kframe_t f, size_t offset)
{
	kframe_t n;
	
	n = frame_new(f->u.pa + offset, f->u.len - offset, f->u.type);
	
	f->u.len = offset;
	n->u.flags = f->u.flags;
	n->u.va = f->u.va + offset;
	
	return n;
}

int
frame_merge(kframe_t a, kframe_t b)
{
	/* TODO. */
	return ERR;
}

