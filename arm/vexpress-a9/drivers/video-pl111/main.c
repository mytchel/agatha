#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <arm/pl111.h>

void
debug(char *fmt, ...)
{
	char s[MESSAGE_LEN] = "pl111:0: ";
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(s + strlen(s), 
			sizeof(s) - strlen(s), 
			fmt, ap);
	va_end(ap);

	mesg(3, (uint8_t *) s, (uint8_t *) s);
}

int
pl111_init(volatile struct pl111_regs *regs)
{
	return ERR;
}

	void
main(void)
{
	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];

	volatile struct pl111_regs *regs;
	char *name = "video0";
	size_t regs_pa, regs_len;
	int ret;

	recv(0, init_m);
	regs_pa = init_m[0];
	regs_len = init_m[1];

	regs = map_addr(regs_pa, regs_len, MAP_DEV|MAP_RW);
	if (regs == nil) {
		debug("failed to map registers!\n");
		return;
	}

	ret = pl111_init(regs);
	if (ret != OK) {
		debug("init failed!\n");
		return;
	}

	while (true)
		;
}

