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

	mesg(2, (uint8_t *) s, (uint8_t *) s);
}

void
main(void)
{
	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];

	size_t regs_pa, regs_len;

	recv(0, init_m);
	regs_pa = init_m[0];
	regs_len = init_m[1];

	regs = map_addr(regs_pa, regs_len, MAP_DEV|MAP_RW);
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

