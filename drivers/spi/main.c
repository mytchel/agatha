#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <proc0.h>
#include <stdarg.h>
#include <string.h>
#include <am335x/spi.h>

static struct spi_regs *
get_spi(void)
{
  struct proc0_dev_req *req;
  struct proc0_dev_resp *resp;
  uint8_t m[MESSAGE_LEN];
  int r, f_id;

  req = (struct proc0_dev_req *) m;
  req->type = proc0_type_dev;
  req->method = proc0_dev_req_method_compat;

  strlcpy(req->kind.compatability, 
      "ti,omap4-mcspi",
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
  struct spi_regs *regs;
  uint8_t m[MESSAGE_LEN];
  int p;

  regs = get_spi();
  if (regs == nil) {
    return;
  }

  while (true) {
    p = recv(m);
    send(p, m);
  }
}

