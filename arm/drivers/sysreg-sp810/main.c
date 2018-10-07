#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <arm/sp810.h>

static volatile struct sp810_regs *regs;

void
debug(char *fmt, ...)
{
	char s[MESSAGE_LEN];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(s, sizeof(s), fmt, ap);
	va_end(ap);

	send(2, (uint8_t *) s);
	recv(2, (uint8_t *) s);
}

void
main(void)
{
	size_t regs_pa, regs_len;

	regs_pa = 0x10000000 + (0 << 12);
	regs_len = 1 << 12;

	regs = request_device(regs_pa, regs_len, MAP_DEV|MAP_RW);
	if (regs == nil) {
		raise();
	}

	debug("sysreg mapped from 0x%x to 0x%x\n", regs_pa, regs);	
	debug("id 0x%x\n", regs->id);	
	debug("mci address 0x%x\n", &regs->mci);	
	debug("mci 0x%x\n", regs->mci);	

	while (true)
		yield();
}

