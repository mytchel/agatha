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
  int pid;

  pid = proc_new();
  if (pid < 0) {
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

