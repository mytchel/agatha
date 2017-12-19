#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <fdt.h>
#include <proc0.h>

#include "proc0.h"

struct kernel_info *k_info;
void *dtb;

static bool
compatable_cb(void *dtb, void *node, void *arg)
{
  struct proc0_dev_compatability_resp *o = arg;
 
  o->phandles[o->nphandles] = fdt_node_phandle(dtb, node);
  frame_merge(o->phandles[o->nphandles], o->nphandles);
  o->nphandles++;

  return true;
}

static void
handle_dev_compatability(int pid, 
    struct proc0_dev_compatability_req *i,
    struct proc0_dev_compatability_resp *o)
{
  memset(o, 0, MESSAGE_LEN);
  o->type = proc0_type_dev_compatability;

  fdt_find_node_compatable(dtb,
      i->compatability,
      &compatable_cb,
      o);
}

  static void
handle_dev(int pid, 
    struct proc0_dev_req *i,
    struct proc0_dev_resp *o)
{
  size_t addr, size;
  void *node;
  int f;

  memset(o, 0, MESSAGE_LEN);
  o->type = proc0_type_dev;

  frame_merge(0, 0);

  node = fdt_find_node_phandle(dtb, i->phandle);
  if (node == nil) {
    /* TODO: what should happen here? */
    memset(o, 0, MESSAGE_LEN);
    return;
  }

  frame_merge(1, 0);
  if (!fdt_node_regs(dtb, node, 0, &addr, &size)) {
    memset(o, 0, MESSAGE_LEN);
    return;
  }

  frame_merge(2, 0);
  f = frame_create(addr, size, F_TYPE_IO);
  if (f < 0) {
    memset(o, 0, MESSAGE_LEN);
    return;
  }

  frame_merge(3, 0);
  if (frame_give(pid, f) != OK) {
    memset(o, 0, MESSAGE_LEN);
    return;
  }

  frame_merge(4, 0);
  o->frames[o->nframes] = f;
  o->nframes++;
}

  static void
handle(int pid, struct proc0_message *i)
{
  uint8_t o[MESSAGE_LEN];

  switch (i->type) {
    case proc0_type_dev_compatability:
      handle_dev_compatability(pid, 
          (struct proc0_dev_compatability_req *) i,
          (struct proc0_dev_compatability_resp *) o);
      break;

    case proc0_type_dev:
      handle_dev(pid, 
          (struct proc0_dev_req *) i,
          (struct proc0_dev_resp *) o);
      break;

    default:
      memset(o, 0, MESSAGE_LEN);
      break;
  }

  send(pid, o);  
}

  void
main(struct kernel_info *k)
{
  uint8_t m[MESSAGE_LEN];
  int f_id, p;

  k_info = k;

  if (!find_tables()) {
    return;
  }

  f_id = frame_create(k_info->dtb_start, k_info->dtb_len, F_TYPE_MEM);
  if (f_id < 0) {
    return;
  }

  dtb = map_frame(f_id, F_MAP_TYPE_PAGE|F_MAP_READ);
  if (dtb == nil) {
    return;
  }

  if (!setup_mem()) {
    return;
  }

  if (!init_procs()) {
    return;
  }

  while (true) {
    p = recv(m);
    handle(p, (struct proc0_message *) m);
  }
}

