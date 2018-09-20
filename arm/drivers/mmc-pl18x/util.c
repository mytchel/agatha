#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <m.h>
#include <stdarg.h>
#include <string.h>

void
debug(char *fmt, ...)
{
	char s[MESSAGE_LEN];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(s, sizeof(s), fmt, ap);
	va_end(ap);

	send(1, (uint8_t *) s);
	recv(1, (uint8_t *) s);
}

