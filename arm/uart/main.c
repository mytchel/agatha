#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <am335x/uart.h>

void
putc(struct uart_regs *uart, char c)
{
  if (c == '\n')
    putc(uart, '\r');
	
	while ((uart->lsr & (1 << 5)) == 0)
		;
	
	uart->hr = c;
}

void
puts(struct uart_regs *uart, const char *c)
{
  while (*c)
    putc(uart, *c++);
}


int
main(void)
{
  struct uart_regs *uart;
  uint8_t m[MESSAGE_LEN];
  int p;

  while (true)
      ;

  puts(uart, (char *) m);

  while (true) {
    p = recv(m);

    m[MESSAGE_LEN-1] = 0;
    puts(uart, (const char *) m);

    send(p, m);
  }

  return OK;
}

