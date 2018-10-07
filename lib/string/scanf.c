#include <types.h>
#include <err.h>
#include <stdarg.h>
#include <string.h>

int
vsscanf(const char *str, const char *fmt, va_list ap)
{
	char *s, *c, t, num[32];
	int n, *i;

	n = 0;
	while (*fmt != 0) {
		if (*fmt != '%') {
			if (*str != *fmt) {
				return n;
			} else {
				fmt++;
				str++;
			}
		}
		fmt++;
		t = *fmt;
		fmt++;

		switch (t) {
		case '%':
			if (*str != '%') {
				return n;
			} else {
				str++;
			}

			break;

		case 'i':
			i = va_arg(ap, int *);

			s = num;
			while (*str != 0 && *str != *fmt) {
				*s++ = *str++;
			}

			*s = 0;

			*i = strtol(num, nil, 10);
			break;

		case 'c':
			c = va_arg(ap, char *);
			*c = *str;
			str++;
			break;

		case 's':
			s = va_arg(ap, char *);

			while (*str != 0 && *str != *fmt) {
				*s++ = *str++;
			}

			*s = 0;
			break;

		default:
			return ERR;
		}

		n++;
	}

	return n;
}

int
sscanf(const char *str, const char *fmt,...)
{
	va_list 	ap;
	int 		i;

	va_start(ap, fmt);
	i = vsscanf(str, fmt, ap);
	va_end(ap);

	return i;
}
