#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <fdt.h>

#include "proc0.h"

struct bundle_proc {
  char name[256];
  size_t len;
};

struct bundle_proc bundled_procs[] = {
#include "bundle.list"
};

void *_binary_arm_proc0_bundle_start;

static bool
init_bundled_proc(char *name,
    size_t start, size_t len)
{
  int f_l1, f_l2, f_user, f_stack;
  uint8_t m[MESSAGE_LEN];
  uint32_t *m_start;
  int pid;

  f_l1 = get_mem_frame(0x4000, 0x4000);
  if (f_l1 < 0) {
    return false;
  }

  f_l2 = get_mem_frame(0x1000, 0x1000);
  if (f_l2 < 0) {
    /* TODO: free f_l1. */
    return false;
  }

  if (map_frame(f_l1, F_MAP_TYPE_PAGE|F_MAP_READ) == nil) {
    return false;
  }

  if (frame_table(f_l1, F_TABLE_L1) != OK) {
    return false;
  }

  if (map_frame(f_l2, F_MAP_TYPE_PAGE|F_MAP_READ) == nil) {
    return false;
  }

  if (frame_table(f_l2, F_TABLE_L2) != OK) {
    return false;
  }

  if (frame_map(f_l1, f_l2, 0, F_MAP_TYPE_TABLE_L2) != OK) {
    return false;
  }

  f_user = frame_create(start, len, F_TYPE_MEM);
  if (f_user < 0) {
    return false;
  }

  f_stack = get_mem_frame(0x4000, 0x1000);
  if (f_stack < 0) {
    return false;
  }

  if (frame_map(f_l2, f_user, (void *) 0x10000, 
        F_MAP_TYPE_PAGE|F_MAP_WRITE|F_MAP_READ) != OK) {
    return false;
  }

  if (frame_map(f_l2, f_stack, (void *) (0x10000 - 0x4000), 
        F_MAP_TYPE_PAGE|F_MAP_WRITE|F_MAP_READ) != OK) {
    return false;
  }

  /* Map tables into new vspace for other procs editing. */

  /* TODO: unmap from this vspace. */

  if (frame_map(f_l2, f_l1, (void *) 0x1000, F_MAP_TYPE_PAGE|F_MAP_READ) != OK) {
    return false;
  }

  if (frame_map(f_l2, f_l2, (void *) 0x5000, F_MAP_TYPE_PAGE|F_MAP_READ) != OK) {
    return false;
  }

  pid = proc_new(f_l1);
  if (pid < 0) {
    return false;
  }

  m_start = (uint32_t *) m;
  m_start[0] = 0x10000;
  m_start[1] = 0x10000;

  if (send(pid, m) != OK) {
    return false;
  }

  return false;
}

bool
init_procs(void)
{
  size_t off;
  int i;

  off = (size_t) &_binary_arm_proc0_bundle_start;

  for (i = 0; 
      i < sizeof(bundled_procs)/sizeof(bundled_procs[0]);
      i++) {

    if (!init_bundled_proc(bundled_procs[i].name, 
          off, bundled_procs[i].len)) {
      return false;
    }

    off += bundled_procs[i].len;
  }

  return true;
}

