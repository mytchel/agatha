#include <types.h>
#include "../bundle.h"
struct bundle_proc bundled_procs[] = {
{ "../../drivers/ping", 12288 },
{ "../../drivers/pong", 12288 },
};
size_t nbundled_procs = sizeof(bundled_procs)/sizeof(bundled_procs[0]);
