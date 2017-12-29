#include "../../kern/head.h"
#include "fns.h"

	int
frame_give(proc_t from, proc_t to, kframe_t f)
{
	kframe_t n, nn;

  debug("frame_give from %i to %i frame %i\n", from->pid, to->pid, f->u.f_id);

  frame_remove(from, f);
  frame_add(to, f);

	n = from->frames;
	while (n != nil) {
		nn = n->next;

		if (n->u.t_id == f->u.f_id || n->u.v_id == f->u.f_id) {
      if (frame_give(from, to, n) != OK) {
        return ERR;
      }
    }

    n = nn;
  }

  return OK;
}

  int
mmu_switch(kframe_t vs)
{
  mmu_load_ttb((uint32_t *) vs->u.pa);
  mmu_invalidate();

  return OK;
}

  int
map_l2(uint32_t *l1, size_t pa, size_t va)
{
  l1[L1X(va)] = pa | L1_COARSE;

  return OK;
}

  int
map_pages(uint32_t *l2, size_t pa, size_t va, 
    size_t len, int ap, bool cache)
{
  uint32_t tex, c, b, o;

  debug("map pages from 0x%h to 0x%h len 0x%h, in table at 0x%h\n",
      pa, va, len, l2);

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

  static int
frame_map_l2(kframe_t t, kframe_t f, size_t va)
{
  uint32_t *l1;
  size_t o;

  if (f->u.t_flags != F_TABLE_L2) {
    return ERR;
  }

  l1 = (uint32_t *) t->u.v_va;

  for (o = 0; o < f->u.len; o += PAGE_SIZE) {
    map_l2(l1, f->u.pa + o, va + o);
  }

  f->u.t_id = t->u.f_id;
  f->u.t_va = va;
  f->u.t_flags |= F_TABLE_MAPPED;

  return OK;
}

  static int
frame_map_sections(kframe_t t, kframe_t f, size_t va, int flags)
{
  uint32_t *l1;
  uint32_t ap;
  int fl, r;

  /* TODO: check if mapping tables for modification. */

  fl = F_MAP_TYPE_SECTION | F_MAP_READ;
  if (f->u.len < SECTION_SIZE) {
    /* TODO: check alignment. */
    return ERR;
  } else if (flags & F_MAP_WRITE) {
    fl |= F_MAP_WRITE;
    ap = AP_RW_RW;
  } else {
    ap = AP_RW_RO;
  }

  l1 = (uint32_t *) t->u.v_va;
  r = map_sections(l1, f->u.pa, va, f->u.len, ap, false);
  if (r != OK) {
    return r;
  }

  f->u.v_id = t->u.f_id;
  f->u.v_va = va;
  f->u.v_flags = fl;

  return OK;
}

  static int
frame_map_pages(kframe_t t, kframe_t f, size_t va, int flags)
{
  uint32_t *l2;
  uint32_t ap;
  int fl, r;

  /* TODO: check if mapping tables for modification. */

  fl = F_MAP_TYPE_PAGE | F_MAP_READ;
  if (flags & F_MAP_WRITE) {
    fl |= F_MAP_WRITE;
    ap = AP_RW_RW;
  } else {
    ap = AP_RW_RO;
  }

  l2 = (uint32_t *) t->u.v_va;
  r = map_pages(l2, f->u.pa, va, f->u.len, ap, false);
  if (r != OK) {
    return r;
  }

  f->u.v_id = t->u.f_id;
  f->u.v_va = va;
  f->u.v_flags = fl;

  return OK;
}

  int
frame_map(kframe_t t, kframe_t f, size_t va, int flags)
{
  if (t->u.t_flags == F_TABLE_NO) {
    return ERR;
  } else if (!(t->u.v_flags & F_MAP_READ)) {
    return ERR;
  }

  /* TODO: check that f can be mapped as flags describes. */

  switch (flags & F_MAP_TYPE_MASK) {
    default:
    case F_MAP_TYPE_PAGE:
      return frame_map_pages(t, f, va, flags);

    case F_MAP_TYPE_SECTION:
      return frame_map_sections(t, f, va, flags);

    case F_MAP_TYPE_TABLE_L2:
      return frame_map_l2(t, f, va);

    case F_MAP_TYPE_TABLE_L1:
      return ERR;
  }
}

  static int
frame_table_l1(kframe_t f, int flags)
{
  if ((f->u.pa & ~(0x4000 - 1)) != f->u.pa) {
    return ERR;
  } else if (f->u.len != 0x4000) {
    return ERR;
  }

  memcpy((uint32_t *) f->u.v_va,
      kernel_ttb,
      0x4000);

  f->u.t_flags = F_TABLE_L1;

  return OK;
}

  static int
frame_table_l2(kframe_t f, int flags)
{
  if (f->u.len < PAGE_SIZE) {
    return ERR;
  }

  memset((uint32_t *) f->u.v_va, 0, f->u.len);

  f->u.t_flags = F_TABLE_L2;

  return OK;
}

int
frame_table(kframe_t t, int flags)
{
  if (t->u.v_flags & F_MAP_WRITE) {
    return ERR;
  } else if (!(t->u.v_flags & F_MAP_READ)) {
    return ERR;
  }

  switch (flags) {
    default:
      return ERR;

    case F_TABLE_L1:
      return frame_table_l1(t, flags);

    case F_TABLE_L2:
      return frame_table_l2(t, flags);
  }
}

