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

#define TCP_TEST_PORT_LOC 4084
#define TCP_TEST_PORT_REM 4032

#define UDP_TEST_PORT_LOC 3000

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
udp_test(int net_pid)
{
	int chan_id, ret, j;
	union net_req rq;
	union net_rsp rp;
	size_t pa, len;
	uint8_t *va;

	uint16_t port_rem;
	uint8_t addr_rem[4];

	len = PAGE_SIZE;
	pa = request_memory(len);
	if (pa == nil) {
		log(LOG_FATAL, "request mem failed");
		return ERR;
	}

	log(LOG_INFO, "start UDP test");

	rq.bind.type = NET_bind_req;
	rq.bind.port_loc = UDP_TEST_PORT_LOC;
	rq.bind.addr_loc[0] = 192;
	rq.bind.addr_loc[1] = 168;
	rq.bind.addr_loc[2] = 10;
	rq.bind.addr_loc[3] = 34;
	rq.bind.proto = NET_proto_udp;

	if (mesg(net_pid, &rq, &rp) != OK) {
		log(LOG_FATAL, "mesg failed!");
		return ERR;
	} else if (rp.bind.ret != OK) {
		log(LOG_FATAL, "open failed %i", rp.bind.ret);
		return ERR;
	}

	chan_id = rp.bind.chan_id;

	for (j = 0; j < 10; j++) {

		int k;
		for (k = 0; k < 100000000; k++)
			;

		if ((ret = give_addr(net_pid, pa, len)) != OK) {
			log(LOG_FATAL, "net give_addr failed %i", ret);
			return ERR;
		}

		rq.read.type = NET_read_req;
		rq.read.chan_id = chan_id;
		rq.read.pa = pa;
		rq.read.pa_len = len;
		rq.read.len = len;
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

		log(LOG_INFO, "read got %i bytes from %i.%i.%i.%i : %i", 
			rp.read.len, 
			rp.read.proto.udp.addr_rem[0],
			rp.read.proto.udp.addr_rem[1],
			rp.read.proto.udp.addr_rem[2],
			rp.read.proto.udp.addr_rem[3],
			rp.read.proto.udp.port_rem);

		size_t b;
		for (b = 0; b < rp.read.len; b++) {
			log(LOG_INFO, "byte %i : 0x%x", b, va[b]);
		}

		unmap_addr(va, len);

		if (rp.read.len == 0) continue;

		port_rem = rp.read.proto.udp.port_rem;
		memcpy(addr_rem, rp.read.proto.udp.addr_rem, 4);

		va = map_addr(pa, len, MAP_RW);

		size_t w_len;
		
		w_len = snprintf((char *) va, len, "hello %i\n", j);

		unmap_addr(va, len);
		
		if ((ret = give_addr(net_pid, pa, len)) != OK) {
			log(LOG_FATAL, "net give_addr failed %i", ret);
			return ERR;
		}

		rq.write.type = NET_write_req;
		rq.write.chan_id = chan_id;
		rq.write.pa = pa;
		rq.write.pa_len = len;
		rq.write.len = w_len;
		rq.write.proto.udp.port_rem = port_rem;
		memcpy(rq.write.proto.udp.addr_rem, addr_rem, 4);

		if (mesg(net_pid, &rq, &rp) != OK) {
			log(LOG_FATAL, "mesg failed!");
			return ERR;
		} else if (rp.write.ret != OK) {
			log(LOG_FATAL, "write failed %i", rp.write.ret);
			break;
		}
	}

	rq.unbind.type = NET_unbind_req;
	rq.unbind.chan_id = chan_id;

	if (mesg(net_pid, &rq, &rp) != OK) {
		log(LOG_FATAL, "mesg failed!");
		return ERR;
	} else if (rp.unbind.ret != OK) {
		log(LOG_FATAL, "unbind failed %i", rp.unbind.ret);
		return ERR;
	}

	return OK;
}

int
tcp_con(int net_pid, int chan_id, int con_id,
		size_t pa, size_t len)
{
	union net_req rq;
	union net_rsp rp;
	int ret, j;
	uint8_t *va;

	bool emptying = false;

	for (j = 0; j < 10; j++) {
		if ((ret = give_addr(net_pid, pa, len)) != OK) {
			log(LOG_FATAL, "net give_addr failed %i", ret);
			return ERR;
		}

		rq.read.type = NET_read_req;
		rq.read.chan_id = chan_id;
		rq.read.proto.tcp.con_id = con_id;
		rq.read.pa = pa;
		rq.read.pa_len = len;
		rq.read.len = len;
		rq.read.timeout_ms = 500;

		if (mesg(net_pid, &rq, &rp) != OK) {
			log(LOG_FATAL, "mesg failed!");
			return ERR;
		} else if (rp.read.ret != OK) {
			log(LOG_FATAL, "read failed %i", rp.read.ret);
			break;
		}

		if (emptying) continue;

		va = map_addr(pa, len, MAP_RW);
		if (va == nil) {
			log(LOG_FATAL, "map addr failed");
			return ERR;
		}

		log(LOG_INFO, "read got %i bytes", rp.read.len);
		size_t b;
		for (b = 0; b < rp.read.len; b++) {
			log(LOG_INFO, "byte %i : 0x%x", b, va[b]);
		}

		size_t w_len;
		
		if (rp.read.len > 0) {
			uint8_t *d = malloc(rp.read.len);
			if (d == nil) {
				log(LOG_WARNING, "out of mem");
				return ERR;
			}

			memcpy(d, va, rp.read.len);
			d[rp.read.len-1] = 0;
			
			w_len = snprintf((char *) va, len, 
						"hello %i, recved %i for '%s'\n", 
						j, rp.read.len, d);

			free(d);
		} else {
			w_len = snprintf((char *) va, len, "hello %i\n", j);
		}

		unmap_addr(va, len);

		if ((ret = give_addr(net_pid, pa, len)) != OK) {
			log(LOG_FATAL, "net give_addr failed %i", ret);
			return ERR;
		}

		rq.write.type = NET_write_req;
		rq.write.chan_id = chan_id;
		rq.write.proto.tcp.con_id = con_id;
		rq.write.pa = pa;
		rq.write.pa_len = len;
		rq.write.len = w_len;

		if (mesg(net_pid, &rq, &rp) != OK) {
			log(LOG_FATAL, "mesg failed!");
			return ERR;
		} else if (rp.write.ret != OK) {
			log(LOG_FATAL, "write failed %i", rp.write.ret);
			emptying = true;
			continue;
		}

		int k;
		for (k = 0; k < 100000000; k++)
			;
	}

	int k;
	for (k = 0; k < 100000000; k++)
		;

	rq.tcp_disconnect.type = NET_tcp_disconnect_req;
	rq.tcp_disconnect.chan_id = chan_id;
	rq.tcp_disconnect.con_id = con_id;

	if (mesg(net_pid, &rq, &rp) != OK) {
		log(LOG_FATAL, "mesg failed!");
		return ERR;
	} else if (rp.tcp_disconnect.ret != OK) {
		log(LOG_FATAL, "disconnect failed %i", rp.tcp_disconnect.ret);
		return ERR;
	}

	return OK;
}

int
tcp_con_test(int net_pid)
{
	int chan_id, con_id;
	union net_req rq;
	union net_rsp rp;
	size_t pa, len;

	len = PAGE_SIZE;
	pa = request_memory(len);
	if (pa == nil) {
		log(LOG_FATAL, "request mem failed");
		return ERR;
	}

	log(LOG_INFO, "start TCP test");

	rq.bind.type = NET_bind_req;
	rq.bind.port_loc = TCP_TEST_PORT_LOC;
	rq.bind.addr_loc[0] = 192;
	rq.bind.addr_loc[1] = 168;
	rq.bind.addr_loc[2] = 10;
	rq.bind.addr_loc[3] = 34;
	rq.bind.proto = NET_proto_tcp;

	if (mesg(net_pid, &rq, &rp) != OK) {
		log(LOG_FATAL, "mesg failed!");
		return ERR;
	} else if (rp.bind.ret != OK) {
		log(LOG_FATAL, "open failed %i", rp.bind.ret);
		return ERR;
	}

	chan_id = rp.bind.chan_id;

	rq.tcp_connect.type = NET_tcp_connect_req;
	rq.tcp_connect.port_rem = TCP_TEST_PORT_REM;
	rq.tcp_connect.chan_id = chan_id;
	rq.tcp_connect.addr_rem[0] = 192;
	rq.tcp_connect.addr_rem[1] = 168;
	rq.tcp_connect.addr_rem[2] = 10;
	rq.tcp_connect.addr_rem[3] = 1;

	if (mesg(net_pid, &rq, &rp) != OK) {
		log(LOG_FATAL, "mesg failed!");
		return ERR;
	} else if (rp.tcp_connect.ret != OK) {
		log(LOG_FATAL, "connect failed %i", rp.tcp_connect.ret);
		return ERR;
	}

	con_id = rp.tcp_connect.con_id;

	tcp_con(net_pid, chan_id, con_id, pa, len);

	rq.unbind.type = NET_unbind_req;
	rq.unbind.chan_id = chan_id;

	if (mesg(net_pid, &rq, &rp) != OK) {
		log(LOG_FATAL, "mesg failed!");
		return ERR;
	} else if (rp.unbind.ret != OK) {
		log(LOG_FATAL, "unbind failed %i", rp.unbind.ret);
		return ERR;
	}

	return OK;
}

int
tcp_serv_test(int net_pid)
{
	int chan_id, con_id;
	union net_req rq;
	union net_rsp rp;
	size_t pa, len;

	len = PAGE_SIZE;
	pa = request_memory(len);
	if (pa == nil) {
		log(LOG_FATAL, "request mem failed");
		return ERR;
	}

	log(LOG_INFO, "start TCP listen test");

	rq.bind.type = NET_bind_req;
	rq.bind.port_loc = TCP_TEST_PORT_LOC;
	rq.bind.addr_loc[0] = 192;
	rq.bind.addr_loc[1] = 168;
	rq.bind.addr_loc[2] = 10;
	rq.bind.addr_loc[3] = 34;
	rq.bind.proto = NET_proto_tcp;

	if (mesg(net_pid, &rq, &rp) != OK) {
		log(LOG_FATAL, "mesg failed!");
		return ERR;
	} else if (rp.bind.ret != OK) {
		log(LOG_FATAL, "bind failed %i", rp.bind.ret);
		return ERR;
	}

	chan_id = rp.bind.chan_id;

	rq.tcp_listen.type = NET_tcp_listen_req;
	rq.tcp_listen.chan_id = chan_id;
	
	if (mesg(net_pid, &rq, &rp) != OK) {
		log(LOG_FATAL, "mesg failed!");
		return ERR;
	} else if (rp.tcp_listen.ret != OK) {
		log(LOG_FATAL, "listen failed %i", rp.tcp_listen.ret);
		return ERR;
	}

	con_id = rp.tcp_listen.con_id;

	log(LOG_INFO, "got con %i", con_id);

	tcp_con(net_pid, chan_id, con_id, pa, len);

	rq.unbind.type = NET_unbind_req;
	rq.unbind.chan_id = chan_id;

	if (mesg(net_pid, &rq, &rp) != OK) {
		log(LOG_FATAL, "mesg failed!");
		return ERR;
	} else if (rp.unbind.ret != OK) {
		log(LOG_FATAL, "unbind failed %i", rp.unbind.ret);
		return ERR;
	}

	return OK;
}

int
main(void)
{
	int net_pid;

	log_init("net_test");

	do {
		net_pid = get_device_pid("net0");
	} while (net_pid < 0);

	log(LOG_INFO, "have net pid %i", net_pid);

	udp_test(net_pid);
	tcp_con_test(net_pid); 
	tcp_serv_test(net_pid);

	return OK;
}

