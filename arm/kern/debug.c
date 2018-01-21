#include "../../kern/head.h"
#include "fns.h"
#include <stdarg.h>

int
debug(const char *fmt, ...)
{
	char str[128];
	va_list ap;
	size_t i;

	if (kernel_devices.debug == nil) {
		return ERR;
	}
	
	va_start(ap, fmt);
	i = vsnprintf(str, sizeof(str), fmt, ap);
	va_end(ap);
	
	if (i > 0) {
		kernel_devices.debug(str);
	}
	
	return i;
}

void
panic(const char *fmt, ...)
{
	char str[128];
	va_list ap;
	size_t i;
	
	if (kernel_devices.debug == nil) {
		raise();
	}

	va_start(ap, fmt);
	i = vsnprintf(str, sizeof(str), fmt, ap);
	va_end(ap);
	
	if (i > 0) {
		kernel_devices.debug(str);
	}

	raise();	
}

