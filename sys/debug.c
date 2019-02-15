#include "head.h"

void (*debug_puts)(const char *) = nil;

void
debug(int code, const char *fmt, ...)
{
  char str[128];
  va_list ap;

	if (debug_puts == nil) {
		return;
	} else if (code > DEBUG_LEVEL) {
		return;
	}

	switch (code) {
		case DEBUG_ERR:
			snprintf(str, sizeof(str),
					"KERNEL ERROR: ");
			break;
		case DEBUG_WARN:
			snprintf(str, sizeof(str),
					"KERNEL WARNING: ");
			break;
		case DEBUG_INFO:
			snprintf(str, sizeof(str),
					"KERNEL INFO: ");
			break;
		case DEBUG_SCHED:
			snprintf(str, sizeof(str),
					"KERNEL SCHED: ");
			break;
		default:
			snprintf(str, sizeof(str),
					"KERNEL SOMETHING (%i?): ", code);
			break;
	}

	va_start(ap, fmt);
	vsnprintf(str, sizeof(str), fmt, ap);
	va_end(ap);

	debug_puts(str);
}

	void
panic(const char *fmt, ...)
{
	char str[128];
	va_list ap;

	snprintf(str, sizeof(str),
			"KERNEL PANIC: ");

	if (debug_puts != nil) {
		va_start(ap, fmt);
		vsnprintf(str + strlen(str), 
				sizeof(str) - strlen(str),
			 	fmt, ap);
		va_end(ap);

		debug_puts(str);
	}

	raise();	
}

