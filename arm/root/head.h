#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <pool.h>
#include "kern.h"

struct service {
	int pid, eid;

	char name[32];
	char bin[256];
	
	struct {
		bool is_device;
		size_t reg, len;
		size_t irqn;

		bool has_regs;
		bool has_irq;

		struct addr_frame *reg_frame;
		int irqn_id;
	} device;

	struct {
		int type;
		char *name;
	} resources[16];
};

struct addr_frame {
	struct addr_frame *next;
	size_t pa, len;
	int table;
	int mapped;
};

struct proc {
	struct addr_frame *frames;

	struct {
		size_t table_pa, mapped_pa;
		uint32_t *table;
		uint32_t *mapped;
	} l1;
};

void
init_mem(void);

size_t
get_ram(size_t len, size_t align);

void
proc_init_l1(uint32_t *l1);


void
init_procs(void);

struct addr_frame *
frame_new(size_t pa, size_t len);

void
frame_free(struct addr_frame *f);

int
proc_give_addr(int pid, struct addr_frame *f);

struct addr_frame *
proc_get_addr(int pid, size_t pa, size_t len);

struct addr_frame *
proc_take_addr(int pid, size_t pa, size_t len);

int
proc_map(int pid, size_t pa, size_t va, size_t len, int flags);

int
proc_unmap(int pid, size_t va, size_t len);


bool
init_bundled_proc(char *name, int priority,
		size_t prog_pa, size_t len,
		int *p_pid, 
		int *p_eid);

void
add_ram(size_t start, size_t len);

void
board_init_ram(void);

extern struct kernel_info *info;

extern int main_eid;

extern struct service services[];
extern size_t nservices;
