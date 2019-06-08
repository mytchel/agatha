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

struct virtio_gpu_dev {
	volatile struct virtio_device *base;
	struct virtio_gpu_display displays[VIRTIO_GPU_MAX_SCANOUTS]; 

	struct virtq controlq;
	struct virtq cursorq;
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

	return OK;
}

void
test(struct virtio_gpu_dev *dev)
{
	struct virtio_gpu_ctrl_hdr *hdr;
	struct virtq_desc *h, *r;
	size_t i_h, i_r;

	size_t fb_len, fb_pa;
	uint8_t *fb_va;

	size_t buf_len, buf_pa;
	uint8_t *buf_va;

	buf_len = PAGE_SIZE;
	buf_pa = request_memory(buf_len);
	if (buf_pa == nil) {
		log(LOG_FATAL, "memory alloc failed");
		return;
	}

	buf_va = map_addr(buf_pa,
			buf_len,
			MAP_DEV|MAP_RW);

	if (buf_va == nil) {
		log(LOG_FATAL, "memory map failed");
		return;
	}

	h = virtq_get_desc(&dev->controlq, &i_h);
	if (h == nil) {
		log(LOG_WARNING, "failed to get descriptor");
		return;
	}

	r = virtq_get_desc(&dev->controlq, &i_r);
	if (h == nil) {
		log(LOG_WARNING, "failed to get descriptor");
		return;
	}

	h->addr = buf_pa;
	h->len = sizeof(struct virtio_gpu_ctrl_hdr);
	h->next = i_r;
	h->flags = VIRTQ_DESC_F_NEXT;

	hdr = (void *) buf_va;
	hdr->type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;
	hdr->flags = 0;

	r->addr = buf_pa + sizeof(struct virtio_gpu_ctrl_hdr);
	r->len = sizeof(struct virtio_gpu_resp_display_info);
	r->next = 0;
	r->flags = VIRTQ_DESC_F_WRITE;

	virtq_push(&dev->controlq, i_h);

	log(LOG_INFO, "sent req");
	uint8_t m[MESSAGE_LEN];
	recv(pid(), m);
	log(LOG_INFO, "got int");

	struct virtq_used_item *e;
	e = virtq_pop(&dev->controlq);
	if (e == nil) {
		log(LOG_FATAL, "got nothing");
		return;
	}

	log(LOG_INFO, "got response len %i", e->len);

	struct virtio_gpu_resp_display_info *info;

	info = (void *) (buf_va + sizeof(struct virtio_gpu_ctrl_hdr));
	log(LOG_INFO, "response hdr type = 0x%x", info->hdr.type);

	log(LOG_INFO, "display 0 enabled = %i",
			info->pmodes[0].enabled);

	log(LOG_INFO, "display 0 at %ix%i size %ix%x",
			info->pmodes[0].r.x,
			info->pmodes[0].r.y,
			info->pmodes[0].r.width,
			info->pmodes[0].r.height);

	memset(dev->displays, 0, sizeof(dev->displays));
	memcpy(&dev->displays[0], &info->pmodes[0], sizeof(dev->displays[0]));

	virtq_free_desc(&dev->controlq, r, i_r);
	virtq_free_desc(&dev->controlq, h, i_h);

	h = virtq_get_desc(&dev->controlq, &i_h);
	if (h == nil) {
		log(LOG_WARNING, "failed to get descriptor");
		return;
	}

	r = virtq_get_desc(&dev->controlq, &i_r);
	if (h == nil) {
		log(LOG_WARNING, "failed to get descriptor");
		return;
	}

	h->addr = buf_pa;
	h->len = sizeof(struct virtio_gpu_resource_create_2d);
	h->next = i_r;
	h->flags = VIRTQ_DESC_F_NEXT;

	struct virtio_gpu_resource_create_2d *rc;

	rc = (void *) buf_va;
	rc->hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
	rc->hdr.flags = 0;
	rc->resource_id = 1;
	rc->format = VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM;
	rc->width = dev->displays[0].r.width;
	rc->height = dev->displays[0].r.height;

	r->addr = buf_pa + sizeof(struct virtio_gpu_resource_create_2d);
	r->len = sizeof(struct virtio_gpu_ctrl_hdr);
	r->next = 0;
	r->flags = VIRTQ_DESC_F_WRITE;

	virtq_push(&dev->controlq, i_h);

	log(LOG_INFO, "sent req");
	recv(pid(), m);
	log(LOG_INFO, "got int");

	e = virtq_pop(&dev->controlq);
	if (e == nil) {
		log(LOG_FATAL, "got nothing");
		return;
	}

	log(LOG_INFO, "got response len %i", e->len);

	hdr = (void *) (buf_va + sizeof(struct virtio_gpu_resource_create_2d));

	if (hdr->type != VIRTIO_GPU_RESP_OK_NODATA) {
		log(LOG_INFO, "bad response type = 0x%x", hdr->type);
		return;
	}

	virtq_free_desc(&dev->controlq, r, i_r);
	virtq_free_desc(&dev->controlq, h, i_h);

	log(LOG_INFO, "resource created");

	fb_len = PAGE_ALIGN(dev->displays[0].r.width * dev->displays[0].r.height * 4);
	fb_pa = request_memory(fb_len);
	if (fb_pa == nil) {
		log(LOG_FATAL, "memory alloc failed");
		return;
	}

	fb_va = map_addr(fb_pa,
			fb_len,
			MAP_DEV|MAP_RW);

	if (fb_va == nil) {
		log(LOG_FATAL, "memory map failed");
		return;
	}

	h = virtq_get_desc(&dev->controlq, &i_h);
	if (h == nil) {
		log(LOG_WARNING, "failed to get descriptor");
		return;
	}

	r = virtq_get_desc(&dev->controlq, &i_r);
	if (h == nil) {
		log(LOG_WARNING, "failed to get descriptor");
		return;
	}

	h->addr = buf_pa;
	h->len = sizeof(struct virtio_gpu_resource_attach_backing)
		+ sizeof(struct virtio_gpu_mem_entry);
	h->next = i_r;
	h->flags = VIRTQ_DESC_F_NEXT;
	
	struct virtio_gpu_resource_attach_backing *rab;
	struct virtio_gpu_mem_entry *mem;

	rab = (void *) buf_va;
	rab->hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
	rab->hdr.flags = 0;
	rab->resource_id = 1;
	rab->nr_entries = 1;

	mem = (void *) (buf_va + sizeof(struct virtio_gpu_resource_attach_backing));
	mem->addr = fb_pa;
	mem->length = dev->displays[0].r.width * dev->displays[0].r.height * 4;
	mem->padding = 0;

	r->addr = buf_pa + 
		sizeof(struct virtio_gpu_resource_attach_backing)
		+ sizeof(struct virtio_gpu_mem_entry);

	r->len = sizeof(struct virtio_gpu_ctrl_hdr);
	r->next = 0;
	r->flags = VIRTQ_DESC_F_WRITE;

	virtq_push(&dev->controlq, i_h);

	log(LOG_INFO, "sent req");
	recv(pid(), m);
	log(LOG_INFO, "got int");

	e = virtq_pop(&dev->controlq);
	if (e == nil) {
		log(LOG_FATAL, "got nothing");
		return;
	}

	log(LOG_INFO, "got response len %i", e->len);

	hdr = (void *) (buf_va 
			+ sizeof(struct virtio_gpu_resource_attach_backing)
			+ sizeof(struct virtio_gpu_mem_entry));

	if (hdr->type != VIRTIO_GPU_RESP_OK_NODATA) {
		log(LOG_INFO, "bad response type = 0x%x", hdr->type);
		return;
	}

	virtq_free_desc(&dev->controlq, r, i_r);
	virtq_free_desc(&dev->controlq, h, i_h);

	log(LOG_INFO, "backing attached");

	memset(fb_va, 0, fb_len);
	int x, y;
	for (x = 50; x < 200; x++) {
		for (y = 50; y < 200; y++) {
			fb_va[(x + y * dev->displays[0].r.width)*4 + 0] = 0x00;
			fb_va[(x + y * dev->displays[0].r.width)*4 + 1] = 0xff;
			fb_va[(x + y * dev->displays[0].r.width)*4 + 2] = 0x00;
			fb_va[(x + y * dev->displays[0].r.width)*4 + 3] = 0xff;
		}
	}

	h = virtq_get_desc(&dev->controlq, &i_h);
	if (h == nil) {
		log(LOG_WARNING, "failed to get descriptor");
		return;
	}

	r = virtq_get_desc(&dev->controlq, &i_r);
	if (h == nil) {
		log(LOG_WARNING, "failed to get descriptor");
		return;
	}

	h->addr = buf_pa;
	h->len = sizeof(struct virtio_gpu_set_scanout);
	h->next = i_r;
	h->flags = VIRTQ_DESC_F_NEXT;
	
	struct virtio_gpu_set_scanout *ss;

	ss = (void *) buf_va;
	ss->hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
	ss->hdr.flags = 0;
	ss->scanout_id = 0;
	ss->resource_id = 1;
	ss->r.x = 0;
	ss->r.y = 0;
	ss->r.width = dev->displays[0].r.width;
	ss->r.height = dev->displays[0].r.height;

	r->addr = buf_pa + sizeof(struct virtio_gpu_set_scanout);
	r->len = sizeof(struct virtio_gpu_ctrl_hdr);
	r->next = 0;
	r->flags = VIRTQ_DESC_F_WRITE;

	virtq_push(&dev->controlq, i_h);

	log(LOG_INFO, "sent req");
	recv(pid(), m);
	log(LOG_INFO, "got int");

	e = virtq_pop(&dev->controlq);
	if (e == nil) {
		log(LOG_FATAL, "got nothing");
		return;
	}

	log(LOG_INFO, "got response len %i", e->len);

	hdr = (void *) (buf_va 
			+ sizeof(struct virtio_gpu_set_scanout));

	if (hdr->type != VIRTIO_GPU_RESP_OK_NODATA) {
		log(LOG_INFO, "bad response type = 0x%x", hdr->type);
		return;
	}

	virtq_free_desc(&dev->controlq, r, i_r);
	virtq_free_desc(&dev->controlq, h, i_h);

	log(LOG_INFO, "scanout resource set");

	h = virtq_get_desc(&dev->controlq, &i_h);
	if (h == nil) {
		log(LOG_WARNING, "failed to get descriptor");
		return;
	}

	r = virtq_get_desc(&dev->controlq, &i_r);
	if (h == nil) {
		log(LOG_WARNING, "failed to get descriptor");
		return;
	}

	h->addr = buf_pa;
	h->len = sizeof(struct virtio_gpu_transfer_to_host_2d);
	h->next = i_r;
	h->flags = VIRTQ_DESC_F_NEXT;
	
	struct virtio_gpu_transfer_to_host_2d *tth;

	tth = (void *) buf_va;
	tth->hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
	tth->hdr.flags = 0;
	tth->r.x = 0;
	tth->r.y = 0;
	tth->r.width = dev->displays[0].r.width;
	tth->r.height = dev->displays[0].r.height;
	tth->offset = 0;
	tth->resource_id = 1;
	tth->padding = 0;

	r->addr = buf_pa + sizeof(struct virtio_gpu_transfer_to_host_2d);
	r->len = sizeof(struct virtio_gpu_ctrl_hdr);
	r->next = 0;
	r->flags = VIRTQ_DESC_F_WRITE;

	virtq_push(&dev->controlq, i_h);

	log(LOG_INFO, "sent req");
	recv(pid(), m);
	log(LOG_INFO, "got int");

	e = virtq_pop(&dev->controlq);
	if (e == nil) {
		log(LOG_FATAL, "got nothing");
		return;
	}

	log(LOG_INFO, "got response len %i", e->len);

	hdr = (void *) (buf_va 
			+ sizeof(struct virtio_gpu_transfer_to_host_2d));

	if (hdr->type != VIRTIO_GPU_RESP_OK_NODATA) {
		log(LOG_INFO, "bad response type = 0x%x", hdr->type);
		return;
	}

	virtq_free_desc(&dev->controlq, r, i_r);
	virtq_free_desc(&dev->controlq, h, i_h);

	log(LOG_INFO, "transfered to host");

	h = virtq_get_desc(&dev->controlq, &i_h);
	if (h == nil) {
		log(LOG_WARNING, "failed to get descriptor");
		return;
	}

	r = virtq_get_desc(&dev->controlq, &i_r);
	if (h == nil) {
		log(LOG_WARNING, "failed to get descriptor");
		return;
	}

	h->addr = buf_pa;
	h->len = sizeof(struct virtio_gpu_resource_flush);
	h->next = i_r;
	h->flags = VIRTQ_DESC_F_NEXT;
	
	struct virtio_gpu_resource_flush *rf;

	rf = (void *) buf_va;
	rf->hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
	rf->hdr.flags = 0;
	rf->r.x = 0;
	rf->r.y = 0;
	rf->r.width = dev->displays[0].r.width;
	rf->r.height = dev->displays[0].r.height;
	rf->resource_id = 1;
	rf->padding = 0;

	r->addr = buf_pa + sizeof(struct virtio_gpu_resource_flush);
	r->len = sizeof(struct virtio_gpu_ctrl_hdr);
	r->next = 0;
	r->flags = VIRTQ_DESC_F_WRITE;

	virtq_push(&dev->controlq, i_h);

	log(LOG_INFO, "sent req");
	recv(pid(), m);
	log(LOG_INFO, "got int");

	e = virtq_pop(&dev->controlq);
	if (e == nil) {
		log(LOG_FATAL, "got nothing");
		return;
	}

	log(LOG_INFO, "got response len %i", e->len);

	hdr = (void *) (buf_va 
			+ sizeof(struct virtio_gpu_resource_flush));

	if (hdr->type != VIRTIO_GPU_RESP_OK_NODATA) {
		log(LOG_INFO, "bad response type = 0x%x", hdr->type);
		return;
	}

	virtq_free_desc(&dev->controlq, r, i_r);
	virtq_free_desc(&dev->controlq, h, i_h);

	log(LOG_INFO, "flushed");

	while (recv(-1, m))
		;
}

	int
main(void)
{
	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];
	char dev_name[MESSAGE_LEN];

	struct virtio_gpu_dev dev;

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

	log(LOG_INFO, "gpu ready");

	test(&dev);

	return ERR;
}

