#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <am335x/uart.h>

void
putc(uart_regs_t uart, char c)
{
  if (c == '\n')
    putc(uart, '\r');
	
	while ((uart->lsr & (1 << 5)) == 0)
		;
	
	uart->hr = c;
}

void
puts(uart_regs_t uart, const char *c)
{
  while (*c)
    putc(uart, *c++);
}


int
main(uart_regs_t uart, size_t len)
{
  uint8_t m[MESSAGE_LEN];
  int p;

  snprintf((char *) m, sizeof(m), 
      "userspace uart at 0x%h, 0x%h\n", uart, len);

  puts(uart, (char *) m);

  while (true) {
    p = recv(m);

    m[MESSAGE_LEN-1] = 0;
    puts(uart, (const char *) m);

    send(p, m);
  }

  return OK;
}

