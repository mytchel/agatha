#include <types.h>
#include <mach.h>
#include "trap.h"

#define MAX_PROCS          512
#define MAX_FRAMES        2048
#define KSTACK_LEN         512

typedef enum {
  INTR_on  = (uint32_t) 0,
  INTR_off = (uint32_t) MODE_DI,
} intr_t;

#include "../../kern/head.h"

