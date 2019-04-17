#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <mach.h>
#include <arm/mmu.h>
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

	rq.find.type = DEV_REG_find;
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
main(void)
{
	union video_req rq;
	union video_rsp rp;
	size_t width, height;
	size_t frame_pa, frame_size;
	uint32_t *frame;
	int dev_pid;

	log_init("display");

	char *dev_name = "lcd0";
	log(LOG_INFO, "find %s", dev_name);

	dev_pid = get_device_pid(dev_name);
	if (dev_pid < 0) {
		log(LOG_FATAL, "failed to get pid of video driver %s", dev_name);
		exit();
	}

	log(LOG_INFO, "%s on pid %i", dev_name, dev_pid);

	rq.connect.type = VIDEO_connect;
	if (mesg(dev_pid, &rq, &rp) != OK || rp.connect.ret != OK) {
		log(LOG_FATAL, "failed to connect to video driver %s", dev_name);
		exit();
	}

	width = rp.connect.width;
	height = rp.connect.height;
	log(LOG_INFO, "starting display on %s %ix%i", dev_name, width, height);

	frame_size = rp.connect.frame_size;
	frame_pa = rp.connect.frame_pa;

	size_t x, y;
	size_t i = 0;

	while (true) {
		log(LOG_INFO, "update frame");

		frame = map_addr(frame_pa, frame_size, MAP_RW|MAP_MEM);
		if (frame == nil) {
			log(LOG_FATAL, "failed to map frame buffer 0x%x", frame_pa);
			exit();
		}

		log(LOG_INFO, "mapped frame 0x%x -> 0x%x", frame_pa, frame);

		memset(frame, 0, frame_size);
		for (x = 0; x < 50; x++) {
			for (y = 0; y < 50; y++) {
				frame[(y + i) * width + (x + i)] = 0x00aa88;
			}
		}

		i++;
		if (i == width - 50 || i == height - 50)
			i = 0;

		unmap_addr(frame, frame_size);

		log(LOG_INFO, "send update");

		if (give_addr(dev_pid, frame_pa, frame_size) != OK) {
			log(LOG_FATAL, "failed to give driver new frame");
			exit();
		}

		rq.update.type = VIDEO_update;
		rq.update.frame_pa = frame_pa;
		rq.update.frame_size = frame_size;
		
		if (mesg(dev_pid, &rq, &rp) != OK || rp.update.ret != OK) {
			log(LOG_FATAL, "failed to update frame!");
			exit();
		}
		
		log(LOG_INFO, "received update");

		frame_pa = rp.update.frame_pa;
	}

	exit();
}

