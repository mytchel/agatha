#include "../../kern/head.h"
#include "fns.h"
#include <stdarg.h>

static void
puts(const char *c)
{

}

int
debug(const char *fmt, ...)
{
	char str[128];
	va_list ap;
	size_t i;
	
	va_start(ap, fmt);
	i = vsnprintf(str, sizeof(str), fmt, ap);
	va_end(ap);
	
	if (i > 0) {
		puts(str);
	}
	
	return i;
}

void
panic(const char *fmt, ...)
{
	char str[128];
	va_list ap;
	size_t i;
	
	va_start(ap, fmt);
	i = vsnprintf(str, sizeof(str), fmt, ap);
	va_end(ap);
	
	if (i > 0) {
		puts(str);
	}
	
	while (true)
		;
}

