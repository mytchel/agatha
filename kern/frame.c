#include "head.h"

static struct kframe frames[MAX_FRAMES] = {0};
static int next_free = 0;
static int next_id = 1;

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

  n->u.v_flags = 0;
	n->u.v_va = 0;
	n->u.v_id = -1;

  n->u.t_flags = 0;
	n->u.t_va = 0;
	n->u.t_id = -1;

	n->next = nil;
	
	n->u.f_id = next_id++;
	
	return n;
}

void
frame_add(proc_t p, kframe_t f)
{
	kframe_t *b;
	
	p->frame_count++;
		
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
frame_find_fid(proc_t p, int f_id)
{
  kframe_t f;

  for (f = p->frames; f != nil; f = f->next) {
    if (f->u.f_id == f_id) {
      return f;
    }
  }

  return nil;
}

kframe_t
frame_find_ind(proc_t p, int ind)
{
  kframe_t f;

  f = p->frames; 
  while (f != nil && ind > 0) {
    f = f->next;
    ind--;
  }

  return f;
}

kframe_t
frame_split(kframe_t f, size_t offset)
{
	kframe_t n;
	
	n = frame_new(f->u.pa + offset, f->u.len - offset, f->u.type);
	
	f->u.len = offset;
	n->u.v_flags = f->u.v_flags;
	n->u.t_flags = f->u.t_flags;
  
  /* TODO: this is wrong. */
	n->u.v_va = f->u.v_va + offset;
	n->u.t_va = f->u.t_va + offset;
	
	return n;
}

int
frame_merge(kframe_t a, kframe_t b)
{
	/* TODO. */
	return ERR;
}

