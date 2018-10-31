#include "head.h"

void (*debug_puts)(const char *) = nil;

  int
debug(const char *fmt, ...)
{
  char str[128];
  va_list ap;
  int i;

	if (debug_puts != nil) {
		va_start(ap, fmt);
		i = vsnprintf(str, sizeof(str), fmt, ap);
		va_end(ap);

		debug_puts(str);
	} else {
		i = -1;
	}

	return i;
}

	void
panic(const char *fmt, ...)
{
	char str[128];
	va_list ap;

	if (debug_puts != nil) {
		va_start(ap, fmt);
		vsnprintf(str, sizeof(str), fmt, ap);
		va_end(ap);

		debug_puts(str);
	}

	raise();	
}

