#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <sysobj.h>
#include <c.h>
#include <mesg.h>

static int cid_next_free = 2;
static int cid_max = 2;

int
get_more_caps(void)
{
	size_t pa, len;

	int n = 255;

	len = 0x1000;
	pa = request_memory(len);
	if (pa == nil) {
		return ERR;
	}

	int cid = cid_next_free;
	
	if (obj_create(cid, pa, len) != OK) {
		return ERR;
	}

	int r = obj_retype(cid, OBJ_caplist, n);
	if (r <= 0) {
		return ERR;
	}

	cid_next_free++;
	cid_max = r + n;

	return OK;
}

int
get_free_cap_id(void)
{
	if (cid_max <= cid_next_free + 1) {
		if (get_more_caps() != OK) {
			return ERR;
		}
	}

	return cid_next_free++;
}

int
obj_create_h(int type, size_t n)
{
	size_t pa, len;

	len = 0x1000;
	pa = request_memory(len);
	if (pa == nil) {
		return ERR;
	}

	if (cid_max <= cid_next_free + 1) {
		if (get_more_caps() != OK) {
			return ERR;
		}
	}
	
	int cid = get_free_cap_id();
	if (cid < 0) {
		return ERR;
	}
	
	if (obj_create(cid, pa, len) != OK) {
		return ERR;
	}

	if (obj_retype(cid, type, n) != OK) {
		return ERR;
	}

	return cid;
}

	int
endpoint_create(void)
{
	return obj_create_h(OBJ_endpoint, 1);
}

