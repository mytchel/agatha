#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <proc0.h>
#include <stdarg.h>
#include <string.h>
#include <dev_reg.h>
#include <video.h>
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

void
frame_update(int dev_pid,
		size_t frame_pa, size_t frame_size,
		size_t width, size_t height)
{
	static int i = 2;
	static int j = 2;
	static int dx = 1, dy = 1;
	union video_req rq;
	uint32_t *frame;
	size_t x, y;

	frame = map_addr(frame_pa, frame_size, MAP_RW|MAP_MEM);
	if (frame == nil) {
		log(LOG_FATAL, "failed to map frame buffer 0x%x", frame_pa);
		exit();
	}

#if 1
	memset(frame, 0, width * height * 4);
#else
	for (x = 2; x < 50; x++) {
		for (y = 2; y < 50; y++) {
			frame[(y + i - 2) * width + (x + j - 2)] = 0x004433;
		}
	}
#endif

	for (x = 2; x < 50; x++) {
		for (y = 2; y < 50; y++) {
			frame[(y + i) * width + (x + j)] = 0x00aa88;
		}
	}

	j += dx;
	i += dy;
	
	if (j == width - 50) {
		dx = -1;
	} else if (j == 2) {
		dx = 1;
	}

	if (i == height - 50) {
		dy = -1;
	} else if (i == 2) {
		dy = 1;
	}

	unmap_addr(frame, frame_size);

	if (give_addr(dev_pid, frame_pa, frame_size) != OK) {
		log(LOG_FATAL, "failed to give driver new frame");
		exit();
	}

	rq.update.type = VIDEO_update_req;
	rq.update.frame_pa = frame_pa;
	rq.update.frame_size = frame_size;

	if (send(dev_pid, &rq) != OK) {
		log(LOG_FATAL, "failed to update frame!");
		exit();
	}
}

	void
main(void)
{
	size_t frame_pas[2], frame_size, frame_pa;
	uint8_t m[MESSAGE_LEN];
	union video_req crq;
	union video_rsp crp;
	size_t width, height;
	bool frame_ready[2] = { true, true };
	int dev_pid, from;

	log_init("display");

	char *dev_name = "lcd0";
	log(LOG_INFO, "find %s", dev_name);

	dev_pid = get_device_pid(dev_name);
	if (dev_pid < 0) {
		log(LOG_FATAL, "failed to get pid of video driver %s", dev_name);
		exit();
	}

	log(LOG_INFO, "%s on pid %i", dev_name, dev_pid);

	crq.connect.type = VIDEO_connect_req;
	if (mesg(dev_pid, &crq, &crp) != OK || crp.connect.ret != OK) {
		log(LOG_FATAL, "failed to connect to video driver %s", dev_name);
		exit();
	}

	width = crp.connect.width;
	height = crp.connect.height;
	frame_size = crp.connect.frame_size;
	log(LOG_INFO, "starting display on %s %ix%i", dev_name, width, height);

	frame_pas[0] = request_memory(frame_size);
	frame_pas[1] = request_memory(frame_size);
	if (frame_pas[0] == nil || frame_pas[1] == nil) {
		log(LOG_FATAL, "failed to get memory for frame buffer");
		exit();
	}

	frame_pa = frame_pas[0];
	frame_ready[0] = false;
	frame_update(dev_pid, frame_pa, frame_size, width, height);

	while (true) {
		if (frame_ready[0]) {
			frame_pa = frame_pas[0];
			frame_ready[0] = false;
		} else if (frame_ready[1]) {
			frame_pa = frame_pas[1];
			frame_ready[1] = false;
		} else {
			frame_pa = nil;
		}

		if (frame_pa != nil) {
			frame_update(dev_pid, frame_pa, frame_size, width, height);
		}

		from = recv(-1, m);

		if (from == dev_pid) {
			union video_rsp *rsp = (void *) m;

			switch (rsp->untyped.type) {
				case VIDEO_update_rsp:
					if (rsp->update.ret != OK) {
						log(LOG_FATAL, "display got error %i", rsp->update.ret);
						break;
					}

					size_t i;
					if (rsp->update.frame_pa == frame_pas[0]) {
						i = 0;
					} else if (rsp->update.frame_pa == frame_pas[1]) {
						i = 1;

					} else {
						log(LOG_FATAL, "bad address from display 0x%x!", rsp->update.frame_pa);
						exit();
					}

					frame_ready[i] = true;

					break;

				default:
					log(LOG_WARNING, "bad message from display");
					break;
			}

		} else {
			log(LOG_INFO, "display got message from %i", from);
		}
	}

	exit();
}

