#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mach.h>
#include <arm/mmu.h>
#include <mesg.h>
#include <stdarg.h>
#include <string.h>
#include <proc0.h>
#include <log.h>
#include <dev_reg.h>
#include <virtio.h>
#include <virtio-gpu.h>
#include <video.h>

struct virtio_gpu_dev {
	volatile struct virtio_device *base;

	struct virtq controlq;
	struct virtq cursorq;

	size_t n_resource_id;

	size_t cmd_len, cmd_pa;
	uint8_t *cmd_va;

	size_t n_scanouts;

	struct {
		size_t x, y;
		size_t w, h;
		bool enabled;

		size_t resource_id;

		int connected_pid;
	} displays[VIRTIO_GPU_MAX_SCANOUTS]; 
};

static uint8_t intr_stack[128]__attribute__((__aligned__(4)));
static void intr_handler(int irqn, void *arg)
{
	struct virtio_gpu_dev *dev = arg;
	uint8_t m[MESSAGE_LEN];

	((uint32_t *) m)[0] = dev->base->interrupt_status;

	dev->base->interrupt_ack = dev->base->interrupt_status;

	send(pid(), m);

	intr_exit();
}

int
claim_irq(void *arg, size_t irqn)
{
	union proc0_req rq;
	union proc0_rsp rp;

	rq.irq_reg.type = PROC0_irq_reg_req;
	rq.irq_reg.irqn = irqn; 
	rq.irq_reg.func = &intr_handler;
	rq.irq_reg.arg = arg;
	rq.irq_reg.sp = &intr_stack[sizeof(intr_stack)];

	if (mesg(PROC0_PID, &rq, &rp) != OK || rp.irq_reg.ret != OK) {
		return ERR;
	}

	return OK;
}

	bool
wait_for_int(struct virtio_gpu_dev *dev)
{
	uint8_t m[MESSAGE_LEN];

	recv(pid(), m);

	return true;
}

bool
init_displays(struct virtio_gpu_dev *dev)
{
	struct virtio_gpu_ctrl_hdr *hdr;
	struct virtq_used_item *e;
	struct virtq_desc *h, *r;
	size_t i_h, i_r, d;
	
	struct virtio_gpu_resp_display_info *info;

	if ((h = virtq_get_desc(&dev->controlq, &i_h)) == nil)
		return false;
	if ((r = virtq_get_desc(&dev->controlq, &i_r)) == nil)
		return false;

	h->addr = dev->cmd_pa;
	h->len = sizeof(struct virtio_gpu_ctrl_hdr);
	h->next = i_r;
	h->flags = VIRTQ_DESC_F_NEXT;

	r->addr = dev->cmd_pa 
		+ sizeof(struct virtio_gpu_ctrl_hdr);
	r->len = sizeof(struct virtio_gpu_resp_display_info);
	r->next = 0;
	r->flags = VIRTQ_DESC_F_WRITE;

	hdr = (void *) dev->cmd_va;
	hdr->type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;
	hdr->flags = 0;

	virtq_push(&dev->controlq, i_h);
	
	wait_for_int(dev);

	e = virtq_pop(&dev->controlq);
	if (e == nil) {
		log(LOG_FATAL, "got nothing");
		return false;
	}

	info = (void *) (dev->cmd_va + sizeof(struct virtio_gpu_ctrl_hdr));
	log(LOG_INFO, "response hdr type = 0x%x", info->hdr.type);

	for (d = 0; d < dev->n_scanouts; d++) {
		log(LOG_INFO, "display %i enabled = %i",
				d, info->pmodes[d].enabled);

		log(LOG_INFO, "display %i at %ix%i size %ix%x",
				d,
				info->pmodes[d].r.x,
				info->pmodes[d].r.y,
				info->pmodes[d].r.width,
				info->pmodes[d].r.height);

		dev->displays[d].enabled = info->pmodes[d].enabled;
		dev->displays[d].x = info->pmodes[d].r.x;
		dev->displays[d].y = info->pmodes[d].r.y;
		dev->displays[d].w = info->pmodes[d].r.width;
		dev->displays[d].h = info->pmodes[d].r.height;
		
		dev->displays[d].connected_pid = -1;
		dev->displays[d].resource_id = 0;
	}

	virtq_free_desc(&dev->controlq, r, i_r);
	virtq_free_desc(&dev->controlq, h, i_h);

	return true;
}

size_t
create_resource(struct virtio_gpu_dev *dev,
		size_t width, size_t height)
{
	struct virtio_gpu_resource_create_2d *rc;
	struct virtio_gpu_ctrl_hdr *hdr;
	struct virtq_used_item *e;
	struct virtq_desc *h, *r;
	size_t i_h, i_r;
	size_t resource_id;
	
	if ((h = virtq_get_desc(&dev->controlq, &i_h)) == nil)
		return 0;
	if ((r = virtq_get_desc(&dev->controlq, &i_r)) == nil)
		return 0;

	resource_id = dev->n_resource_id++;

	h->addr = dev->cmd_pa;
	h->len = sizeof(struct virtio_gpu_resource_create_2d);
	h->next = i_r;
	h->flags = VIRTQ_DESC_F_NEXT;

	r->addr = dev->cmd_pa 
		+ sizeof(struct virtio_gpu_resource_create_2d);
	r->len = sizeof(struct virtio_gpu_ctrl_hdr);
	r->next = 0;
	r->flags = VIRTQ_DESC_F_WRITE;

	rc = (void *) dev->cmd_va;
	rc->hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
	rc->hdr.flags = 0;
	rc->resource_id = resource_id;
	rc->format = VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM;
	rc->width = width;
	rc->height = height;

	virtq_push(&dev->controlq, i_h);
	
	wait_for_int(dev);

	e = virtq_pop(&dev->controlq);
	if (e == nil) {
		log(LOG_FATAL, "got nothing");
		return 0;
	}

	hdr = (void *) (dev->cmd_va + sizeof(struct virtio_gpu_resource_create_2d));

	if (hdr->type != VIRTIO_GPU_RESP_OK_NODATA) {
		log(LOG_INFO, "bad response type = 0x%x", hdr->type);
		return 0;
	}

	virtq_free_desc(&dev->controlq, r, i_r);
	virtq_free_desc(&dev->controlq, h, i_h);

	return resource_id;
}

bool
resource_attach_back(struct virtio_gpu_dev *dev,
		size_t resource_id,
		size_t addr, size_t len)
{
	struct virtio_gpu_resource_attach_backing *rab;
	struct virtio_gpu_mem_entry *mem;
	
	struct virtio_gpu_ctrl_hdr *hdr;
	struct virtq_used_item *e;
	struct virtq_desc *h, *r;
	size_t i_h, i_r;
	
	if ((h = virtq_get_desc(&dev->controlq, &i_h)) == nil)
		return false;
	if ((r = virtq_get_desc(&dev->controlq, &i_r)) == nil)
		return false;

	h->addr = dev->cmd_pa;
	h->len = sizeof(struct virtio_gpu_resource_attach_backing)
		+ sizeof(struct virtio_gpu_mem_entry);
	h->next = i_r;
	h->flags = VIRTQ_DESC_F_NEXT;

	r->addr = dev->cmd_pa 
		+ sizeof(struct virtio_gpu_resource_attach_backing)
		+ sizeof(struct virtio_gpu_mem_entry);
	r->len = sizeof(struct virtio_gpu_ctrl_hdr);
	r->next = 0;
	r->flags = VIRTQ_DESC_F_WRITE;

	rab = (void *) dev->cmd_va;
	rab->hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
	rab->hdr.flags = 0;
	rab->resource_id = resource_id;
	rab->nr_entries = 1;

	mem = (void *) (dev->cmd_va 
			+ sizeof(struct virtio_gpu_resource_attach_backing));

	mem->addr = addr;
	mem->length = len;
	mem->padding = 0;

	virtq_push(&dev->controlq, i_h);
	
	wait_for_int(dev);

	e = virtq_pop(&dev->controlq);
	if (e == nil) {
		log(LOG_FATAL, "got nothing");
		return false;
	}

	hdr = (void *) (dev->cmd_va 
			+ sizeof(struct virtio_gpu_resource_attach_backing)
			+ sizeof(struct virtio_gpu_mem_entry));

	if (hdr->type != VIRTIO_GPU_RESP_OK_NODATA) {
		log(LOG_INFO, "bad response type = 0x%x", hdr->type);
		return false;
	}

	virtq_free_desc(&dev->controlq, r, i_r);
	virtq_free_desc(&dev->controlq, h, i_h);

	return true;
}

bool
resource_detach_back(struct virtio_gpu_dev *dev,
		size_t resource_id)
{
	struct virtio_gpu_resource_detach_backing *rdb;
	
	struct virtio_gpu_ctrl_hdr *hdr;
	struct virtq_used_item *e;
	struct virtq_desc *h, *r;
	size_t i_h, i_r;
	
	if ((h = virtq_get_desc(&dev->controlq, &i_h)) == nil)
		return false;
	if ((r = virtq_get_desc(&dev->controlq, &i_r)) == nil)
		return false;

	h->addr = dev->cmd_pa;
	h->len = sizeof(struct virtio_gpu_resource_detach_backing);
	h->next = i_r;
	h->flags = VIRTQ_DESC_F_NEXT;

	r->addr = dev->cmd_pa 
		+ sizeof(struct virtio_gpu_resource_detach_backing);
	r->len = sizeof(struct virtio_gpu_ctrl_hdr);
	r->next = 0;
	r->flags = VIRTQ_DESC_F_WRITE;

	rdb = (void *) dev->cmd_va;
	rdb->hdr.type = VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING;
	rdb->hdr.flags = 0;
	rdb->resource_id = resource_id;

	virtq_push(&dev->controlq, i_h);
	
	wait_for_int(dev);

	e = virtq_pop(&dev->controlq);
	if (e == nil) {
		log(LOG_FATAL, "got nothing");
		return false;
	}

	hdr = (void *) (dev->cmd_va 
			+ sizeof(struct virtio_gpu_resource_detach_backing));

	if (hdr->type != VIRTIO_GPU_RESP_OK_NODATA) {
		log(LOG_INFO, "bad response type = 0x%x", hdr->type);
		return false;
	}

	virtq_free_desc(&dev->controlq, r, i_r);
	virtq_free_desc(&dev->controlq, h, i_h);

	return true;
}

bool
set_scanout(struct virtio_gpu_dev *dev,
		size_t resource, size_t scanout,
		size_t x, size_t y, size_t width, size_t height)
{
	struct virtio_gpu_set_scanout *ss;
	
	struct virtio_gpu_ctrl_hdr *hdr;
	struct virtq_used_item *e;
	struct virtq_desc *h, *r;
	size_t i_h, i_r;
	
	if ((h = virtq_get_desc(&dev->controlq, &i_h)) == nil)
		return false;
	if ((r = virtq_get_desc(&dev->controlq, &i_r)) == nil)
		return false;

	h->addr = dev->cmd_pa;
	h->len = sizeof(struct virtio_gpu_set_scanout);
	h->next = i_r;
	h->flags = VIRTQ_DESC_F_NEXT;

	r->addr = dev->cmd_pa 
		+ sizeof(struct virtio_gpu_set_scanout);
	r->len = sizeof(struct virtio_gpu_ctrl_hdr);
	r->next = 0;
	r->flags = VIRTQ_DESC_F_WRITE;

	ss = (void *) dev->cmd_va;
	ss->hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
	ss->hdr.flags = 0;
	ss->scanout_id = scanout;
	ss->resource_id = resource;
	ss->r.x = x;
	ss->r.y = y;
	ss->r.width = width;
	ss->r.height = height;

	virtq_push(&dev->controlq, i_h);
	
	wait_for_int(dev);

	e = virtq_pop(&dev->controlq);
	if (e == nil) {
		log(LOG_FATAL, "got nothing");
		return false;
	}

	hdr = (void *) (dev->cmd_va + sizeof(struct virtio_gpu_set_scanout));

	if (hdr->type != VIRTIO_GPU_RESP_OK_NODATA) {
		log(LOG_INFO, "bad response type = 0x%x", hdr->type);
		return false;
	}

	virtq_free_desc(&dev->controlq, r, i_r);
	virtq_free_desc(&dev->controlq, h, i_h);

	return true;
}

	bool
transfer_resource(struct virtio_gpu_dev *dev,
		size_t resource, size_t offset,
		size_t x, size_t y,
		size_t width, size_t height)
{
	struct virtio_gpu_transfer_to_host_2d *tth;
	
	struct virtio_gpu_ctrl_hdr *hdr;
	struct virtq_used_item *e;
	struct virtq_desc *h, *r;
	size_t i_h, i_r;
	
	if ((h = virtq_get_desc(&dev->controlq, &i_h)) == nil)
		return false;
	if ((r = virtq_get_desc(&dev->controlq, &i_r)) == nil)
		return false;

	h->addr = dev->cmd_pa;
	h->len = sizeof(struct virtio_gpu_transfer_to_host_2d);
	h->next = i_r;
	h->flags = VIRTQ_DESC_F_NEXT;

	r->addr = dev->cmd_pa 
		+ sizeof(struct virtio_gpu_transfer_to_host_2d);
	r->len = sizeof(struct virtio_gpu_ctrl_hdr);
	r->next = 0;
	r->flags = VIRTQ_DESC_F_WRITE;

	tth = (void *) dev->cmd_va;
	tth->hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
	tth->hdr.flags = 0;
	tth->r.x = x;
	tth->r.y = y;
	tth->r.width = width;
	tth->r.height = height;
	tth->offset = offset;
	tth->padding = 0;
	tth->resource_id = resource;

	virtq_push(&dev->controlq, i_h);
	
	wait_for_int(dev);

	e = virtq_pop(&dev->controlq);
	if (e == nil) {
		log(LOG_FATAL, "got nothing");
		return false;
	}

	hdr = (void *) (dev->cmd_va + sizeof(struct virtio_gpu_transfer_to_host_2d));

	if (hdr->type != VIRTIO_GPU_RESP_OK_NODATA) {
		log(LOG_INFO, "bad response type = 0x%x", hdr->type);
		return false;
	}

	virtq_free_desc(&dev->controlq, r, i_r);
	virtq_free_desc(&dev->controlq, h, i_h);

	return true;
}

	bool
flush_resource(struct virtio_gpu_dev *dev,
		size_t resource, 
		size_t x, size_t y, size_t width, size_t height)
{
	struct virtio_gpu_resource_flush *rf;
	
	struct virtio_gpu_ctrl_hdr *hdr;
	struct virtq_used_item *e;
	struct virtq_desc *h, *r;
	size_t i_h, i_r;
	
	if ((h = virtq_get_desc(&dev->controlq, &i_h)) == nil)
		return false;
	if ((r = virtq_get_desc(&dev->controlq, &i_r)) == nil)
		return false;

	h->addr = dev->cmd_pa;
	h->len = sizeof(struct virtio_gpu_resource_flush);
	h->next = i_r;
	h->flags = VIRTQ_DESC_F_NEXT;

	r->addr = dev->cmd_pa 
		+ sizeof(struct virtio_gpu_resource_flush);
	r->len = sizeof(struct virtio_gpu_ctrl_hdr);
	r->next = 0;
	r->flags = VIRTQ_DESC_F_WRITE;

	rf = (void *) dev->cmd_va;
	rf->hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
	rf->hdr.flags = 0;
	rf->r.x = x;
	rf->r.y = y;
	rf->r.width = width;
	rf->r.height = height;
	rf->resource_id = resource;
	rf->padding = 0;

	virtq_push(&dev->controlq, i_h);
	
	wait_for_int(dev);

	e = virtq_pop(&dev->controlq);
	if (e == nil) {
		log(LOG_FATAL, "got nothing");
		return false;
	}

	hdr = (void *) (dev->cmd_va + sizeof(struct virtio_gpu_resource_flush));

	if (hdr->type != VIRTIO_GPU_RESP_OK_NODATA) {
		log(LOG_INFO, "bad response type = 0x%x", hdr->type);
		return false;
	}

	virtq_free_desc(&dev->controlq, r, i_r);
	virtq_free_desc(&dev->controlq, h, i_h);

	return true;
}

	int
init_dev(struct virtio_gpu_dev *dev)
{
	if (dev->base->magic != VIRTIO_DEV_MAGIC) {
		log(LOG_FATAL, "virtio register magic bad 0x%x != expected 0x%x",
				dev->base->magic, VIRTIO_DEV_MAGIC);
		return ERR;
	}

	log(LOG_INFO, "virtio version = 0x%x",
			dev->base->version);

	if (dev->base->device_id != VIRTIO_DEV_TYPE_GPU) {
		log(LOG_FATAL, "virtio type bad %i", dev->base->device_id);
		return ERR;
	}

	dev->base->status = VIRTIO_ACK;
	dev->base->status |= VIRTIO_DRIVER;

	/* TODO: negotiate */

	struct virtio_gpu_config *config = 
		(struct virtio_gpu_config *) dev->base->config;

	log(LOG_INFO, "gpu config events read  = 0x%x", config->events_read);
	log(LOG_INFO, "gpu config events clear = 0x%x", config->events_clear);
	log(LOG_INFO, "gpu config num scanouts = 0x%x", config->num_scanouts);

	dev->n_scanouts = config->num_scanouts;

	dev->base->device_features_sel = 0;
	dev->base->driver_features_sel = 0;
	dev->base->driver_features = 0;

	log(LOG_INFO, "gpu features = 0x%x", dev->base->device_features);

	dev->base->driver_features_sel = 1;
	dev->base->driver_features = 0;

	dev->base->status |= VIRTIO_FEATURES_OK;
	if (!(dev->base->status & VIRTIO_FEATURES_OK)) {
		log(LOG_FATAL, "virtio feature set rejected!");
		return ERR;
	}

	dev->base->page_size = PAGE_SIZE;

	if (!virtq_init(&dev->controlq, dev->base, 0, 16)) {
		log(LOG_FATAL, "virtio queue init failed");
		return ERR;
	}

	if (!virtq_init(&dev->cursorq, dev->base, 1, 16)) {
		log(LOG_FATAL, "virtio queue init failed");
		return ERR;
	}

	dev->base->status |= VIRTIO_DRIVER_OK;

	if (dev->base->status != 0xf) {
		log(LOG_FATAL, "virtio status bad");
		return ERR;
	}

	dev->cmd_len = PAGE_SIZE;
	dev->cmd_pa = request_memory(dev->cmd_len);
	if (dev->cmd_pa == nil) {
		log(LOG_FATAL, "memory alloc failed");
		return ERR;
	}

	dev->cmd_va = map_addr(dev->cmd_pa,
			dev->cmd_len,
			MAP_DEV|MAP_RW);

	if (dev->cmd_va == nil) {
		log(LOG_FATAL, "memory map failed");
		return ERR;
	}

	dev->n_resource_id = 1;

	return OK;
}

static int
set_frame(struct virtio_gpu_dev *dev, 
		int resource, 
		size_t pa, size_t len,
		size_t w, size_t h)
{
	if (!resource_attach_back(dev, resource, pa, len))
	{
		return ERR;
	}

	if (!transfer_resource(dev, resource,
				0, 0, 0, w, h))
 	{
		return ERR;
	}

	if (!flush_resource(dev, resource,
				0, 0, w, h))
	{
		return ERR;
	}

	if (!resource_detach_back(dev, resource)) {
		return ERR;
	}

	return OK;
}

static void
handle_connect(struct virtio_gpu_dev *dev, 
		int from, union video_req *rq)
{
	union video_rsp rp;

	log(LOG_INFO, "%i is connecting", from);

	rp.connect.type = VIDEO_connect_rsp;

	if (dev->displays[0].connected_pid != -1) {
		rp.connect.ret = ERR;
		send(from, &rp);
		return;
	}

	rp.connect.frame_size = PAGE_ALIGN(dev->displays[0].w * dev->displays[0].h * 4);
	rp.connect.width = dev->displays[0].w;
	rp.connect.height = dev->displays[0].h;

	dev->displays[0].resource_id = create_resource(dev, 
			dev->displays[0].w, dev->displays[0].h);

	if (dev->displays[0].resource_id == 0) {
		rp.connect.ret = ERR;
		send(from, &rp);
		return;
	}

	if (!set_scanout(dev, dev->displays[0].resource_id, 0, 
				0, 0,
				dev->displays[0].w, dev->displays[0].h)) 
	{
		rp.connect.ret = ERR;
		send(from, &rp);
		return;
	}

	rp.connect.ret = OK;
	
	dev->displays[0].connected_pid = from;

	send(from, &rp);
}

	static void
handle_update(struct virtio_gpu_dev *dev, 
		int from, union video_req *rq)
{
	union video_rsp rp;

	rp.update.type = VIDEO_update_rsp;
	rp.update.frame_pa = rq->update.frame_pa;
	rp.update.frame_size = rq->update.frame_size;

	if (dev->displays[0].connected_pid == from) {
		rp.update.ret = set_frame(dev, dev->displays[0].resource_id, 
				rq->update.frame_pa, rq->update.frame_size,
				dev->displays[0].w, dev->displays[0].h);
	} else {
		rp.update.ret	= ERR;
	}

	give_addr(from, 
			rq->update.frame_pa,
			rq->update.frame_size);

	send(from, &rp);
}

	int
main(void)
{
	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];
	char dev_name[MESSAGE_LEN];

	struct virtio_gpu_dev dev;
	int from;

	size_t regs_pa, regs_len;
	size_t regs_off, regs_va;
	size_t irqn;

	recv(0, init_m);
	regs_pa = init_m[0];
	regs_len = init_m[1];
	irqn = init_m[2];

	recv(0, dev_name);

	log_init(dev_name);

	regs_off = regs_pa - PAGE_ALIGN_DN(regs_pa);

	regs_va = (size_t) map_addr(PAGE_ALIGN_DN(regs_pa), 
			PAGE_ALIGN(regs_len), 
			MAP_DEV|MAP_RW);

	if (regs_va == nil) {
		log(LOG_FATAL, "virtio-gpu failed to map registers 0x%x 0x%x!",
				regs_pa, regs_len);
		return ERR;
	}

	log(LOG_INFO, "virtio-gpu on 0x%x 0x%x with irq %i",
			regs_pa, regs_len, irqn);

	dev.base = (struct virtio_device *) ((size_t) regs_va + regs_off);

	log(LOG_INFO, "virtio-gpu mapped 0x%x -> 0x%x",
			regs_pa, dev.base);

	init_dev(&dev);

	if (claim_irq(&dev, irqn) != OK) {
		log(LOG_FATAL, "failed to register interrupt %i", irqn);
		return ERR;
	}

	log(LOG_INFO, "read displays");

	if (!init_displays(&dev)) {
		return ERR;
	}

	log(LOG_INFO, "gpu ready");

	union dev_reg_req drq;
	union dev_reg_rsp drp;

	drq.type = DEV_REG_register_req;
	drq.reg.pid = pid();
	memcpy(drq.reg.name, dev_name, sizeof(drq.reg.name));

	if (mesg(DEV_REG_PID, &drq, &drp) != OK) {
		log(LOG_FATAL, "failed to message dev reg at pid %i", DEV_REG_PID);
		exit(ERR);
	}

	if (drp.reg.ret != OK) {
		log(LOG_FATAL, "failed to register to dev reg");
		exit(drp.reg.ret);
	}

	while (true) {
		union video_req rq;
		from = recv(-1, &rq);

		switch (rq.type) {
			case VIDEO_connect_req:
				handle_connect(&dev, from, &rq);
				break;

			case VIDEO_update_req:
				handle_update(&dev, from, &rq);
				break;
		}
	}	

	return ERR;
}

