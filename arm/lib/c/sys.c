#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <sysobj.h>
#include <c.h>
#include <mesg.h>

int
obj_create_h(int type, size_t n)
{
	size_t pa, len;

	len = 0x1000;
	pa = request_memory(len);
	if (pa == nil) {
		return ERR;
	}

	int c;
	
	c = obj_create(pa, len);
	if (c < 0) {
		return ERR;
	}

	if (obj_retype(c, type, n) != OK) {
		return ERR;
	}

	return c;
}

	int
endpoint_create(void)
{
	return obj_create_h(OBJ_endpoint, 1);
}

