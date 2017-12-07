#include "head.h"
#include "fns.h"
#include <stdarg.h>
#include <am335x/uart.h>

static struct uart_regs *uart;

void
init_uart(void *regs)
{
	uart = (struct uart_regs *) regs;
}

void
putc(char c)
{
  if (c == '\n')
    putc('\r');
	
	while ((uart->lsr & (1 << 5)) == 0)
		;
	
	uart->hr = c;
}

void
puts(const char *c)
{
  while (*c)
    putc(*c++);
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

