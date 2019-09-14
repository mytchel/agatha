#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <proc0.h>
#include <mmu.h>
#include <stdarg.h>
#include <string.h>
#include <dev_reg.h>
#include <net.h>
#include <log.h>

int
get_device_pid(char *name)
{
	union dev_reg_req rq;
	union dev_reg_rsp rp;

	rq.find.type = DEV_REG_find_req;
	rq.find.block = true;
	snprintf(rq.find.name, sizeof(rq.find.name),
			"%s", name);

	if (mesg(DEV_REG_PID, &rq, &rp) != OK) {
		return ERR;
	} else if (rp.find.ret != OK) {
		return ERR;
	}

	return rp.find.pid;
}

int
main(void)
{
	int net_pid;
	int ret;

	log_init("net_test");

	do {
		net_pid = get_device_pid("net0");
	} while (net_pid < 0);

	log(LOG_INFO, "have net pid %i", net_pid);

	union net_req rq;
	union net_rsp rp;

	rq.open.type = NET_open_req;
	rq.open.port = 4242;
	rq.open.addr[0] = 192;
	rq.open.addr[1] = 168;
	rq.open.addr[2] = 10;
	rq.open.addr[3] = 1;
	rq.open.proto = NET_UDP;

	if (mesg(net_pid, &rq, &rp) != OK) {
		log(LOG_FATAL, "mesg failed!");
		return ERR;
	} else if (rp.open.ret != OK) {
		log(LOG_FATAL, "open failed %i", rp.open.ret);
		return ERR;
	}

	int chan_id = rp.open.id;

	size_t pa, len;
	uint8_t *va;

	len = PAGE_SIZE;
	pa = request_memory(len);
	if (pa == nil) {
		log(LOG_FATAL, "request mem failed");
		return ERR;
	}

	int j;
	for (j = 0; j < 10; j++) {
		va = map_addr(pa, len, MAP_RW);
		if (va == nil) {
			log(LOG_FATAL, "map addr failed");
			return ERR;
		}

		size_t w_len;
		
		w_len = snprintf((char *) va, len, "hello %i\n", j);

		unmap_addr(va, len);
		
		if ((ret = give_addr(net_pid, pa, len)) != OK) {
			log(LOG_FATAL, "net give_addr failed %i", ret);
			return ERR;
		}

		rq.write.type = NET_write_req;
		rq.write.id = chan_id;
		rq.write.pa = pa;
		rq.write.len = len;
		rq.write.w_len = w_len;

		if (mesg(net_pid, &rq, &rp) != OK) {
			log(LOG_FATAL, "mesg failed!");
			return ERR;
		} else if (rp.write.ret != OK) {
			log(LOG_FATAL, "write failed %i", rp.write.ret);
			return ERR;
		}

		if ((ret = give_addr(net_pid, pa, len)) != OK) {
			log(LOG_FATAL, "net give_addr failed %i", ret);
			return ERR;
		}

		rq.read.type = NET_read_req;
		rq.read.id = chan_id;
		rq.read.pa = pa;
		rq.read.len = len;
		rq.read.r_len = len;
		rq.read.timeout_ms = 500;

		if (mesg(net_pid, &rq, &rp) != OK) {
			log(LOG_FATAL, "mesg failed!");
			return ERR;
		} else if (rp.read.ret != OK) {
			log(LOG_FATAL, "read failed %i", rp.read.ret);
			return ERR;
		}

		va = map_addr(pa, len, MAP_RW);
		if (va == nil) {
			log(LOG_FATAL, "map addr failed");
			return ERR;
		}

		log(LOG_INFO, "read got %i bytes", rp.read.r_len);
		size_t b;
		for (b = 0; b < rp.read.r_len; b++) {
			log(LOG_INFO, "byte %i : 0x%x", b, va[b]);
		}
		
		unmap_addr(va, len);

		int k;
		for (k = 0; k < 100000000; k++)
			;
	}

	rq.close.type = NET_close_req;
	rq.close.id = chan_id;

	if (mesg(net_pid, &rq, &rp) != OK) {
		log(LOG_FATAL, "mesg failed!");
		return ERR;
	} else if (rp.close.ret != OK) {
		log(LOG_FATAL, "close failed %i", rp.close.ret);
		return ERR;
	}

	return OK;
}

