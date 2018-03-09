#include "../../kern/head.h"
#include "../kern/fns.h"
#include <stdarg.h>
#include <fdt.h>

  static void
putc(char c)
{
  
}

  void
puts(const char *c)
{
  while (*c)
    putc(*c++);
}

  void
init_serial(void)
{
  
}

  void
map_serial(void *dtb)
{

}

