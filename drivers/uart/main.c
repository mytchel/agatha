#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <proc0.h>
#include <stdarg.h>
#include <string.h>
#include <am335x/uart.h>

static void
putc(struct uart_regs *uart, char c)
{
  if (c == '\n')
    putc(uart, '\r');
	
	while ((uart->lsr & (1 << 5)) == 0)
		;
	
	uart->hr = c;
}

static void
puts(struct uart_regs *uart, const char *c)
{
  while (*c)
    putc(uart, *c++);
}

static struct uart_regs *
get_uart(void)
{
  struct proc0_dev_req *req;
  struct proc0_dev_resp *resp;
  uint8_t m[MESSAGE_LEN];
  int r, f_id;

  req = (struct proc0_dev_req *) m;
  req->type = proc0_type_dev;
  req->method = proc0_dev_req_method_compat;

  strlcpy(req->kind.compatability, 
      "ti,omap3-uart",
      sizeof(req->kind.compatability));

  if (send(0, m) != OK) {
    return nil;
  }

  do {
    r = recv(m);
    if (r < 0) {
      return nil;
    }
  } while (r != 0);

  resp = (struct proc0_dev_resp *) m;
  if (resp->nframes == 0) {
    return nil;
  }

  f_id = resp->frames[0];

  return frame_map_free(f_id, F_MAP_READ|F_MAP_WRITE);
}

void
main(void)
{
  struct uart_regs *uart;
  uint8_t m[MESSAGE_LEN];
  int p;

  uart = get_uart();
  if (uart == nil) {
    return;
  }

  puts(uart, "Hello, World!\n");

  while (true) {
    p = recv(m);

    m[MESSAGE_LEN-1] = 0;
    puts(uart, (const char *) m);

    send(p, m);
  }
}

