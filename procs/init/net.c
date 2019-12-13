#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <proc0.h>
#include <mmu.h>
#include <stdarg.h>
#include <string.h>
#include <net.h>
#include <log.h>

#define TCP_TEST_PORT_LOC 4087
#define TCP_TEST_PORT_REM 4086

#define UDP_TEST_PORT_LOC 3000

int
udp_test(int net_eid)
{
	int chan_id, j;
	union net_req rq;
	union net_rsp rp;
	size_t len;
	int fid;
	uint8_t *va;

	uint16_t port_rem;
	uint8_t addr_rem[4];

	len = PAGE_SIZE;
	fid = request_memory(len, 0x1000);
	if (fid < 0) {
		log(LOG_FATAL, "request mem failed");
		return ERR;
	}

	log(LOG_INFO, "start UDP test");

	rq.bind.type = NET_bind;
	rq.bind.port_loc = UDP_TEST_PORT_LOC;
	rq.bind.addr_loc[0] = 192;
	rq.bind.addr_loc[1] = 168;
	rq.bind.addr_loc[2] = 10;
	rq.bind.addr_loc[3] = 34;
	rq.bind.proto = NET_proto_udp;

	if (mesg(net_eid, &rq, &rp) != OK) {
		log(LOG_FATAL, "mesg failed!");
		return ERR;
	} else if (rp.bind.ret != OK) {
		log(LOG_FATAL, "open failed %i", rp.bind.ret);
		return ERR;
	}

	chan_id = rp.bind.chan_id;

	for (j = 0; j < 5; j++) {

		int k;
		for (k = 0; k < 100000000; k++)
			;

		rq.read.type = NET_read;
		rq.read.chan_id = chan_id;
		rq.read.len = len;
		rq.read.timeout_ms = 500;

		if (mesg_cap(net_eid, &rq, &rp, fid) != OK) {
			log(LOG_FATAL, "mesg failed!");
			return ERR;
		} else if (rp.read.ret != OK) {
			log(LOG_FATAL, "read failed %i", rp.read.ret);
			return ERR;
		}

		va = frame_map_anywhere(fid);
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

		unmap_addr(fid, va);

		if (rp.read.len > 0) {
			port_rem = rp.read.proto.udp.port_rem;
			memcpy(addr_rem, rp.read.proto.udp.addr_rem, 4);
		} else {
			port_rem = 4000;
			addr_rem[0] = 192;
			addr_rem[1] = 168;
			addr_rem[2] = 10;
			addr_rem[3] = 1;
		}

		va = frame_map_anywhere(fid);

		size_t w_len;
		
		w_len = snprintf((char *) va, len, "hello %i\n", j);

		unmap_addr(fid, va);
		
		rq.write.type = NET_write;
		rq.write.chan_id = chan_id;
		rq.write.len = w_len;
		rq.write.proto.udp.port_rem = port_rem;
		memcpy(rq.write.proto.udp.addr_rem, addr_rem, 4);

		if (mesg_cap(net_eid, &rq, &rp, fid) != OK) {
			log(LOG_FATAL, "mesg failed!");
			return ERR;
		} else if (rp.write.ret != OK) {
			log(LOG_FATAL, "write failed %i", rp.write.ret);
			break;
		}
	}

	rq.unbind.type = NET_unbind;
	rq.unbind.chan_id = chan_id;

	if (mesg(net_eid, &rq, &rp) != OK) {
		log(LOG_FATAL, "mesg failed!");
		return ERR;
	} else if (rp.unbind.ret != OK) {
		log(LOG_FATAL, "unbind failed %i", rp.unbind.ret);
		return ERR;
	}

	release_memory(fid);

	return OK;
}

int
tcp_con(int net_eid, int chan_id, int con_id,
		int fid)
{
	union net_req rq;
	union net_rsp rp;
	int j;
	uint8_t *va;
	size_t len = 0x1000;

	bool emptying = false;

	for (j = 0; j < 50; j++) {
		log(LOG_INFO, "tcp con do read %i", j);

		rq.read.type = NET_read;
		rq.read.chan_id = chan_id;
		rq.read.proto.tcp.con_id = con_id;
		rq.read.len = len;
		rq.read.timeout_ms = 500;

		if (mesg_cap(net_eid, &rq, &rp, fid) != OK) {
			log(LOG_FATAL, "mesg failed!");
			return ERR;
		} else if (rp.read.ret != OK) {
			log(LOG_FATAL, "read failed %i", rp.read.ret);
			break;
		}

		if (emptying) continue;

		va = frame_map_anywhere(fid);
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

		unmap_addr(fid, va);

		log(LOG_INFO, "tcp con do write %i", j);

		rq.write.type = NET_write;
		rq.write.chan_id = chan_id;
		rq.write.proto.tcp.con_id = con_id;
		rq.write.len = w_len;

		if (mesg_cap(net_eid, &rq, &rp, fid) != OK) {
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

	log(LOG_INFO, "tcp con done, disconnect");

	int k;
	for (k = 0; k < 100000000; k++)
		;

	rq.tcp_disconnect.type = NET_tcp_disconnect;
	rq.tcp_disconnect.chan_id = chan_id;
	rq.tcp_disconnect.con_id = con_id;

	if (mesg(net_eid, &rq, &rp) != OK) {
		log(LOG_FATAL, "mesg failed!");
		return ERR;
	} else if (rp.tcp_disconnect.ret != OK) {
		log(LOG_FATAL, "disconnect failed %i", rp.tcp_disconnect.ret);
		return ERR;
	}

	return OK;
}

int
tcp_con_test(int net_eid)
{
	int chan_id, con_id;
	union net_req rq;
	union net_rsp rp;
	size_t len;
	int fid;

	len = PAGE_SIZE;
	fid = request_memory(len, 0x1000);
	if (fid < 0) {
		log(LOG_FATAL, "request mem failed");
		return ERR;
	}

	log(LOG_INFO, "start TCP test");

	rq.bind.type = NET_bind;
	rq.bind.port_loc = TCP_TEST_PORT_LOC;
	rq.bind.addr_loc[0] = 192;
	rq.bind.addr_loc[1] = 168;
	rq.bind.addr_loc[2] = 10;
	rq.bind.addr_loc[3] = 34;
	rq.bind.proto = NET_proto_tcp;

	if (mesg(net_eid, &rq, &rp) != OK) {
		log(LOG_FATAL, "mesg failed!");
		return ERR;
	} else if (rp.bind.ret != OK) {
		log(LOG_FATAL, "open failed %i", rp.bind.ret);
		return ERR;
	}

	chan_id = rp.bind.chan_id;

	rq.tcp_connect.type = NET_tcp_connect;
	rq.tcp_connect.port_rem = TCP_TEST_PORT_REM;
	rq.tcp_connect.chan_id = chan_id;
	rq.tcp_connect.addr_rem[0] = 192;
	rq.tcp_connect.addr_rem[1] = 168;
	rq.tcp_connect.addr_rem[2] = 10;
	rq.tcp_connect.addr_rem[3] = 1;

	if (mesg(net_eid, &rq, &rp) != OK) {
		log(LOG_FATAL, "mesg failed!");
		return ERR;
	} else if (rp.tcp_connect.ret != OK) {
		log(LOG_FATAL, "connect failed %i", rp.tcp_connect.ret);
		return ERR;
	}

	con_id = rp.tcp_connect.con_id;

	tcp_con(net_eid, chan_id, con_id, fid);

	rq.unbind.type = NET_unbind;
	rq.unbind.chan_id = chan_id;

	if (mesg(net_eid, &rq, &rp) != OK) {
		log(LOG_FATAL, "mesg failed!");
		return ERR;
	} else if (rp.unbind.ret != OK) {
		log(LOG_FATAL, "unbind failed %i", rp.unbind.ret);
		return ERR;
	}

	release_memory(fid);

	return OK;
}

int
tcp_serv_test(int net_eid)
{
	int chan_id, con_id;
	union net_req rq;
	union net_rsp rp;
	size_t len;
	int fid;

	len = PAGE_SIZE;
	fid = request_memory(len, 0x1000);
	if (fid < 0) {
		log(LOG_FATAL, "request mem failed");
		return ERR;
	}

	log(LOG_INFO, "start TCP listen test");

	rq.bind.type = NET_bind;
	rq.bind.port_loc = TCP_TEST_PORT_LOC;
	rq.bind.addr_loc[0] = 192;
	rq.bind.addr_loc[1] = 168;
	rq.bind.addr_loc[2] = 10;
	rq.bind.addr_loc[3] = 34;
	rq.bind.proto = NET_proto_tcp;

	if (mesg(net_eid, &rq, &rp) != OK) {
		log(LOG_FATAL, "mesg failed!");
		return ERR;
	} else if (rp.bind.ret != OK) {
		log(LOG_FATAL, "bind failed %i", rp.bind.ret);
		return ERR;
	}

	chan_id = rp.bind.chan_id;

	rq.tcp_listen.type = NET_tcp_listen;
	rq.tcp_listen.chan_id = chan_id;
	
	if (mesg(net_eid, &rq, &rp) != OK) {
		log(LOG_FATAL, "mesg failed!");
		return ERR;
	} else if (rp.tcp_listen.ret != OK) {
		log(LOG_FATAL, "listen failed %i", rp.tcp_listen.ret);
		return ERR;
	}

	con_id = rp.tcp_listen.con_id;

	log(LOG_INFO, "got con %i", con_id);

	tcp_con(net_eid, chan_id, con_id, fid);

	rq.unbind.type = NET_unbind;
	rq.unbind.chan_id = chan_id;

	if (mesg(net_eid, &rq, &rp) != OK) {
		log(LOG_FATAL, "mesg failed!");
		return ERR;
	} else if (rp.unbind.ret != OK) {
		log(LOG_FATAL, "unbind failed %i", rp.unbind.ret);
		return ERR;
	}

	release_memory(fid);

	return OK;
}

void
test_net(void)
{
	union proc0_req prq;
	union proc0_rsp prp;
	int net_eid;

	net_eid = kcap_alloc();
	if (net_eid < 0) {
		log(LOG_FATAL, "failed to alloc cap");
		exit(ERR);
	}

	prq.get_resource.type = PROC0_get_resource;
	prq.get_resource.resource_type = RESOURCE_type_net;
	
	mesg_cap(CID_PARENT, &prq, &prp, net_eid);
	if (prp.get_resource.ret != OK) {
		log(LOG_FATAL, "failed to get net device");
		exit(ERR);
	}

	log_init("net_test");

	udp_test(net_eid);
	/*tcp_con_test(net_eid);*/
	tcp_serv_test(net_eid);
}

