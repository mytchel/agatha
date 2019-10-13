#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <sysobj.h>
#include <c.h>
#include <mesg.h>

	int
endpoint_create(void)
{
	int c;

	c = obj_retype(1, OBJ_endpoint, 1);
	return c;
}

