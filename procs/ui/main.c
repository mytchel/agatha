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
#include <evdev.h>
#include <evdev_codes.h>

struct ui_dev {
	int gpu_pid;
	int key_pid;
	int ptr_pid;
	
	size_t width, height;
	size_t frame_pas[2], frame_size, frame_pa;
	bool frame_ready[2];
};

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
frame_update(int gpu_pid,
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
		exit(ERR);
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

	if (give_addr(gpu_pid, frame_pa, frame_size) != OK) {
		log(LOG_FATAL, "failed to give driver new frame");
		exit(ERR);
	}

	rq.update.type = VIDEO_update_req;
	rq.update.frame_pa = frame_pa;
	rq.update.frame_size = frame_size;

	if (send(gpu_pid, &rq) != OK) {
		log(LOG_FATAL, "failed to update frame!");
		exit(ERR);
	}
}

	void
init_gpu(struct ui_dev *dev, char *name)
{
	union video_req grq;
	union video_rsp grp;
	
	log(LOG_INFO, "find %s", name);
	if ((dev->gpu_pid = get_device_pid(name)) < 0) {
		log(LOG_FATAL, "failed to get pid of video driver %s", name);
		exit(ERR);
	}

	log(LOG_INFO, "%s on pid %i", name, dev->gpu_pid);
	grq.connect.type = VIDEO_connect_req;
	if (mesg(dev->gpu_pid, &grq, &grp) != OK || grp.connect.ret != OK) {
		log(LOG_FATAL, "failed to connect to video driver %s", name);
		exit(ERR);
	}

	dev->width = grp.connect.width;
	dev->height = grp.connect.height;
	dev->frame_size = grp.connect.frame_size;
	log(LOG_INFO, "starting display on %s %ix%i", 
			name, dev->width, dev->height);

	dev->frame_pas[0] = request_memory(dev->frame_size);
	dev->frame_pas[1] = request_memory(dev->frame_size);
	if (dev->frame_pas[0] == nil || dev->frame_pas[1] == nil) {
		log(LOG_FATAL, "failed to get memory for frame buffer");
		exit(ERR);
	}

	dev->frame_ready[0] = false;
	dev->frame_ready[1] = true;

	frame_update(dev->gpu_pid, 
			dev->frame_pas[0], dev->frame_size, 
			dev->width, dev->height);
}

int
init_evdev(char *name)
{
	union evdev_req erq;
	union evdev_rsp erp;
	int p;

	log(LOG_INFO, "find %s", name);
	if ((p = get_device_pid(name)) < 0) {
		log(LOG_FATAL, "failed to get pid of %s", name);
		exit(ERR);
	}
	
	log(LOG_INFO, "%s on pid %i", name, p);

	erq.connect.type = EVDEV_connect_req;
	if (mesg(p, &erq, &erp) != OK || erp.connect.ret != OK) {
		log(LOG_FATAL, "failed to connect to %s", name);
		exit(ERR);
	}

	return p;
}

	void
update(struct ui_dev *dev)
{
	size_t frame_pa;

	if (dev->frame_ready[0]) {
		frame_pa = dev->frame_pas[0];
		dev->frame_ready[0] = false;
	} else if (dev->frame_ready[1]) {
		frame_pa = dev->frame_pas[1];
		dev->frame_ready[1] = false;
	} else {
		frame_pa = nil;
	}

	if (frame_pa != nil) {
		frame_update(dev->gpu_pid, 
				frame_pa, dev->frame_size, 
				dev->width, dev->height);
	}
}

void
handle_gpu_msg(struct ui_dev *dev, uint8_t *m)
{
	union video_rsp *rsp = (void *) m;
	size_t i;

	switch (*((uint32_t *) m)) {
		case VIDEO_update_rsp:
			if (rsp->update.ret != OK) {
				log(LOG_FATAL, "display got error %i", rsp->update.ret);
				break;
			}

			if (rsp->update.frame_pa == dev->frame_pas[0]) {
				i = 0;
			} else if (rsp->update.frame_pa == dev->frame_pas[1]) {
				i = 1;

			} else {
				log(LOG_FATAL, "bad address from display 0x%x!", rsp->update.frame_pa);
				exit(ERR);
			}

			dev->frame_ready[i] = true;

			break;

		default:
			log(LOG_WARNING, "bad message from display");
			break;
	}
}

void
handle_evdev_msg(struct ui_dev *dev, int from,
		uint8_t *m)
{
	union evdev_msg *e;

	switch (*((uint32_t *) m)) {
		case EVDEV_event_msg:
			e = (void *) m;
			log(LOG_INFO, "event from %i %i\t%i\t%i",
					from, 
					e->event.event_type, 
					e->event.code, 
					e->event.value);
		default:
			break;
	}
}

	void
main(void)
{
	uint8_t m[MESSAGE_LEN];
	int from;

	struct ui_dev dev;

	log_init("ui");

	char *gpu_name = "gpu0";
	char *key_name = "key0";
	char *ptr_name = "mouse0";

	init_gpu(&dev, gpu_name);
	dev.key_pid = init_evdev(key_name);
	dev.ptr_pid = init_evdev(ptr_name);

	while (true) {
		update(&dev);

		from = recv(-1, m);

		if (from == dev.gpu_pid) {
			handle_gpu_msg(&dev, m);
		} else if (from == dev.key_pid ||from == dev.ptr_pid) {
			handle_evdev_msg(&dev, from, m);

		} else {
			log(LOG_INFO, "display got message from %i", from);
		}
	}

	exit(OK);
}

