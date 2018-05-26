#include <types.h>
#include "../bundle.h"
struct bundle_proc bundled_procs[] = {
{ "/home/mytch/dev/os/arm/virt/../../drivers/ping", 12288 },
{ "/home/mytch/dev/os/arm/virt/../../drivers/pong", 12288 },
};
size_t nbundled_procs = sizeof(bundled_procs)/sizeof(bundled_procs[0]);
