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
	size_t frame_id;
	size_t frame_pas[2], frame_size, frame_pa;

	int32_t px, py;
	uint32_t pc;

	uint32_t c;
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
frame_flush(struct ui_dev *dev, size_t frame_id)
{
	union video_req rq;

	if (give_addr(dev->gpu_pid, dev->frame_pas[frame_id], dev->frame_size) != OK) {
		log(LOG_FATAL, "failed to give driver new frame");
		exit(ERR);
	}

	rq.draw.type = VIDEO_draw_req;
	rq.draw.frame_id = frame_id;

	if (send(dev->gpu_pid, &rq) != OK) {
		log(LOG_FATAL, "failed to update frame!");
		exit(ERR);
	}
}

void
frame_draw(struct ui_dev *dev, size_t frame_id)
{
	static int i = 2;
	static int j = 2;
	static int dx = 1, dy = 1;

	uint8_t *frame;
	size_t x, y;

	frame = map_addr(dev->frame_pas[frame_id], dev->frame_size, MAP_RW|MAP_MEM);
	if (frame == nil) {
		log(LOG_FATAL, "failed to map frame buffer 0x%x", dev->frame_pas[frame_id]);
		exit(ERR);
	}

	memset(frame, 0, dev->width * dev->height * 4);

	for (x = 0; x < 5 && dev->px + x < dev->width; x++) {
		for (y = 0; y < 5 && dev->py + y < dev->height; y++) {
			frame[((y + dev->py) * dev->width + (x + dev->px)) * 4 + 0] = 0;
			frame[((y + dev->py) * dev->width + (x + dev->px)) * 4 + 1] = (dev->pc >> 16) & 0xff;
			frame[((y + dev->py) * dev->width + (x + dev->px)) * 4 + 2] = (dev->pc >> 8) & 0xff;
			frame[((y + dev->py) * dev->width + (x + dev->px)) * 4 + 3] = (dev->pc >> 0) & 0xff;
		}
	}

	for (x = 2; x < 50; x++) {
		for (y = 2; y < 50; y++) {
			frame[((y + j) * dev->width + (x + i)) * 4 + 0] = 0;
			frame[((y + j) * dev->width + (x + i)) * 4 + 1] = (dev->c >> 16) & 0xff;
			frame[((y + j) * dev->width + (x + i)) * 4 + 2] = (dev->c >> 8) & 0xff;
			frame[((y + j) * dev->width + (x + i)) * 4 + 3] = (dev->c >> 0) & 0xff;
		}
	}

	j += dy;
	i += dx;
	
	if (i == dev->width - 50) {
		dx = -1;
	} else if (x == 2) {
		dx = 1;
	}

	if (j == dev->height - 50) {
		dy = -1;
	} else if (j == 2) {
		dy = 1;
	}

	unmap_addr(frame, dev->frame_size);
}

	void
init_gpu(struct ui_dev *dev, char *name)
{
	union video_req grq;
	union video_rsp grp;
	
	log(LOG_INFO, "find %s", name);
	if ((dev->gpu_pid = get_device_pid(name)) < 0) {
		log(LOG_FATAL, "failed to get pid of gpu %s", name);
		exit(ERR);
	}

	log(LOG_INFO, "%s on pid %i", name, dev->gpu_pid);
	grq.connect.type = VIDEO_connect_req;
	if (mesg(dev->gpu_pid, &grq, &grp) != OK || grp.connect.ret != OK) {
		log(LOG_FATAL, "failed to connect to gpu %s", name);
		exit(ERR);
	}

	dev->width = grp.connect.width;
	dev->height = grp.connect.height;
	dev->frame_size = grp.connect.frame_size;
	log(LOG_INFO, "starting gpu on %s %ix%i", 
			name, dev->width, dev->height);

	log(LOG_INFO, "get first frame");

	grq.create_frame.type = VIDEO_create_frame_req;
	if (mesg(dev->gpu_pid, &grq, &grp) != OK || grp.create_frame.ret != OK) {
		log(LOG_FATAL, "gpu failed to create frame");
		exit(ERR);
	}

	dev->frame_pas[grp.create_frame.frame_id] = grp.create_frame.frame_pa;

	log(LOG_INFO, "frame %i at 0x%x",
			grp.create_frame.frame_id, grp.create_frame.frame_pa);

	log(LOG_INFO, "get second frame");
	grq.create_frame.type = VIDEO_create_frame_req;
	if (mesg(dev->gpu_pid, &grq, &grp) != OK || grp.create_frame.ret != OK) {
		log(LOG_FATAL, "gpu failed to create frame");
		exit(ERR);
	}

	log(LOG_INFO, "frame %i at 0x%x",
			grp.create_frame.frame_id, grp.create_frame.frame_pa);

	dev->frame_pas[grp.create_frame.frame_id] = grp.create_frame.frame_pa;

	frame_draw(dev, 0);	
	frame_flush(dev, 0);	
	frame_draw(dev, 1);	
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
handle_gpu_msg(struct ui_dev *dev, uint8_t *m)
{
	union video_rsp *rsp = (void *) m;

	switch (*((uint32_t *) m)) {
		case VIDEO_draw_rsp:
			if (rsp->draw.ret != OK) {
				log(LOG_FATAL, "gpu got error %i", rsp->draw.ret);
				break;
			}

			frame_flush(dev, (rsp->draw.frame_id + 1) % 2);
			frame_draw(dev, rsp->draw.frame_id);
			break;

		default:
			log(LOG_WARNING, "bad message from gpu");
			break;
	}
}

	void
handle_evdev_rel(struct ui_dev *dev, 
		uint32_t code, int32_t value)
{
	switch (code) {
		case REL_X:
			dev->px += value;
			break;
		case REL_Y:
			dev->py += value;
			break;
	}

	if (dev->px < 0) dev->px = 0;
	else if (dev->px > dev->width) dev->px = dev->width;

	if (dev->py < 0) dev->py = 0;
	else if (dev->py > dev->height) dev->py = dev->height;
}

	void
handle_evdev_key(struct ui_dev *dev, 
		uint32_t code, uint32_t value)
{
	switch (code) {
		case BTN_LEFT:
			if (value) dev->pc &= ~0xff0000;
			else dev->pc |= 0xff0000;
			break;

		case BTN_RIGHT:
			if (value) dev->pc &= ~0x00ff00;
			else dev->pc |= 0x00ff00;
			break;

		case BTN_MIDDLE:
			if (value) dev->pc &= ~0x0000ff;
			else dev->pc |= 0x0000ff;
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
			switch (e->event.event_type) {
				case EV_REL:
					handle_evdev_rel(dev, 
							e->event.code, 
							e->event.value);
					break;

				case EV_KEY:
					handle_evdev_key(dev, 
							e->event.code, 
							e->event.value);
					break;
			}

			break;

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

	dev.px = dev.width / 2;
	dev.py = dev.height / 2;
	dev.pc = 0xffffff;

	dev.c = 0x00aa88;

	while (true) {
		from = recv(-1, m);

		if (from == dev.gpu_pid) {
			handle_gpu_msg(&dev, m);
		} else if (from == dev.key_pid ||from == dev.ptr_pid) {
			handle_evdev_msg(&dev, from, m);

		} else {
			log(LOG_INFO, "message from unknown pid %i", from);
		}
	}

	exit(OK);
}

