#include <types.h>
#include <stdarg.h>
#include <string.h>

long
strtol(const char *nptr, char **endptr, int base)
{
  long n = 0;
  bool positive = true;

  while (isspace(*nptr))
    nptr++;

  if (*nptr == '+') {
    positive = true;
    nptr++;
  } else if (*nptr == '-') {
    positive = false;
    nptr++;
  }

  while (*nptr != 0 && !isspace(*nptr)) {
    n = n * base;
    n += (*nptr - '0');
    nptr++;
  }

  if (!positive) {
    n = -n;
  }
  
  return n;
}
