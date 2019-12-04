#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <sysobj.h>
#include <c.h>
#include <mesg.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <pool.h>
#include "kern.h"

struct service {
	char name[32];
	char bin[256];
	
	struct {
		bool is_device;
		size_t reg, len;
		size_t irqn;

		bool has_regs;
		bool has_irq;

		int reg_cid;
		int irq_cid;
	} device;

	struct {
		int type;
		char *name;
	} resources[16];
		
	int pid;
	int listen_eid;
	int connect_eid;
};

void
init_mem(void);

void
init_procs(void);

bool
init_bundled_proc(uint8_t *code,
	char *name, int priority, size_t len);

void
add_ram(size_t start, size_t len);

int
create_frame(size_t pa, size_t len, int type);

void
board_init_ram(void);

extern struct kernel_info *info;

extern int main_eid;

extern struct service services[];
extern size_t nservices;

