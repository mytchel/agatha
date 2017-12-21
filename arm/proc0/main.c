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
handle_dev_node(struct proc0_dev_resp *o, void *dtb, void *node)
{
  size_t addr, size;
  int f;

  if (!fdt_node_regs(dtb, node, 0, &addr, &size)) {
    return false;
  }

  f = frame_create(addr, size, F_TYPE_IO);
  if (f < 0) {
    return false;
  }

  o->frames[o->nframes] = f;
  o->nframes++;

  return true; 
}

static bool
compatable_cb(void *dtb, void *node, void *arg)
{
  struct proc0_dev_resp *o = arg;

  handle_dev_node(o, dtb, node);
  
  /* Stop. */
  return false;
}

static void
handle_dev(int pid, 
    struct proc0_dev_req *i,
    struct proc0_dev_resp *o)
{
  void *node;
  int f;

  memset(o, 0, MESSAGE_LEN);
  o->type = proc0_type_dev;

  switch (i->method) {
    case proc0_dev_req_method_phandle:
       node = fdt_find_node_phandle(dtb, i->kind.phandle);
       if (node != nil) {
         handle_dev_node(o, dtb, node);
       }

     break;

    case proc0_dev_req_method_compat:
      fdt_find_node_compatable(dtb,
          i->kind.compatability,
          &compatable_cb,
          o);

      break;

    case proc0_dev_req_method_path:
    default:
      break;
  }

  for (f = 0; f < o->nframes; f++) {
    if (frame_give(pid, o->frames[f]) != OK) {
      memset(o, 0, MESSAGE_LEN);
    }
  }
}

  static void
handle(int pid, struct proc0_message *i)
{
  uint8_t o[MESSAGE_LEN];

  switch (i->type) {
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

