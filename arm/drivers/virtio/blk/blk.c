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
#include <block.h>

struct device {
	volatile struct virtio_device *dev;
	struct virtq q;

	size_t buffer_pa, buffer_len;
	void *buffer_va;
};

size_t virtq_size(size_t qsz)
{
	return PAGE_ALIGN(sizeof(struct virtq_desc) * qsz + sizeof(uint16_t)*(3+qsz)) 
		+ PAGE_ALIGN(sizeof(uint16_t)*3 + sizeof(struct virtq_used_item)*qsz);
}

struct virtq_desc *
virtq_get_desc(struct virtq *q, size_t *index)
{
	size_t i;

	for (i = 0; i < q->size; i++) {
		if (q->desc[i].len == 0) {
			*index = i;
			/* mark as in use */
			q->desc[i].len = 1; 
			return &q->desc[i];
		}
	}

	return nil;
}

void
virtq_send(struct virtq *q, size_t index)
{
	struct virtq_desc *d;
	size_t idx;
 
	idx = q->avail->idx % q->size;
	q->avail->rings[idx] = index;
	q->avail->idx++;

	q->dev->queue_notify = 0;

	uint8_t m[MESSAGE_LEN];
	recv(pid(), m);

	do {
		d = &q->desc[index];
		d->len = 0;
		index = d->next;
	} while (d->flags & VIRTQ_DESC_F_NEXT);
}

bool
virtq_init(struct virtq *q, 
		volatile struct virtio_device *dev, 
		size_t queue_index,
		size_t num)
{
	size_t queue_pa, queue_va, queue_len;
	size_t avail_off, used_off;

	dev->queue_sel = queue_index;
	
	if (dev->queue_num_max < num) {
		log(LOG_FATAL, "queue len %i is too large for queue %i (max = %i)",
				num, queue_index, dev->queue_num_max);
		return false;
	}

	queue_len = PAGE_ALIGN(virtq_size(num));

	log(LOG_INFO, "need 0x%x bytes for queue of size 0x%x", 
			queue_len, num);

	queue_pa = request_memory(queue_len);
	if (queue_pa == nil) {
		log(LOG_FATAL, "virtio queue memory alloc failed");
		return false;
	}

	queue_va = (size_t) map_addr(queue_pa, queue_len, MAP_DEV|MAP_RW);
	if (queue_va == nil) {
		log(LOG_FATAL, "virtio queue memory map failed");
		release_addr(queue_pa, queue_len);
		return false;
	}

	memset((void *) queue_va, 0, queue_len);

	avail_off = sizeof(struct virtq_desc) * num;
	used_off = PAGE_ALIGN(sizeof(struct virtq_desc) * num + 
			sizeof(uint16_t)*(3+num));

	q->size = num;
	q->dev = dev;
	q->desc = (void *) queue_va;
	q->avail = (void *) (queue_va + avail_off);
	q->used = (void *) (queue_va + used_off);

	dev->queue_num = num;
	dev->queue_used_align = PAGE_SIZE;
	dev->queue_pfn = queue_pa >> PAGE_SHIFT;

	return true;
}

int
virtio_blk_op(struct device *dev,
		size_t sector, bool write,
		size_t buf_pa, size_t buf_len)
{
	struct virtq_desc *d[3];
	size_t index[3];

	d[0] = virtq_get_desc(&dev->q, &index[0]);
	d[1] = virtq_get_desc(&dev->q, &index[1]);
	d[2] = virtq_get_desc(&dev->q, &index[2]);

	if (d[0] == nil || d[1] == nil || d[2] == nil) {
		log(LOG_WARNING, "failed to get virtio descs");
		return ERR;
	}

	d[0]->addr = dev->buffer_pa;
	d[0]->len = 4+4+8;
	d[0]->next = index[1];
	d[0]->flags = VIRTQ_DESC_F_NEXT;
	
	d[1]->addr = buf_pa;
	d[1]->len = buf_len;
	d[1]->next = index[2];

	if (!write) {
		d[1]->flags = VIRTQ_DESC_F_WRITE;
	}

	d[1]->flags |= VIRTQ_DESC_F_NEXT;
	
	d[2]->addr = dev->buffer_pa + 16;	
	d[2]->len = 1;
	d[2]->flags = VIRTQ_DESC_F_WRITE;

	struct virtio_blk_req *r = dev->buffer_va;
	
	if (write) {
		r->type = VIRTIO_BLK_T_OUT;
	} else {
		r->type = VIRTIO_BLK_T_IN;
	}

	r->reserved = 0;
	r->sector = sector;

	virtq_send(&dev->q, index[0]);

	return (int) ((uint8_t *) dev->buffer_va)[16];
}

static uint8_t intr_stack[128]__attribute__((__aligned__(4)));
static void intr_handler(int irqn, void *arg)
{
	struct device *dev = arg;
	uint8_t m[MESSAGE_LEN];

	dev->dev->interrupt_ack = dev->dev->interrupt_status;

	send(pid(), m);

	intr_exit();
}

int
read_blocks(struct block_dev *dev,
		size_t pa, size_t start, size_t n)
{
	log(LOG_INFO, "read 0x%x blocks from 0x%x",
			n, start);

	return virtio_blk_op(dev->arg,
			start, false,
			pa, n * dev->block_size);
}

int
write_blocks(struct block_dev *dev,
		size_t pa, size_t start, size_t n)
{
	return virtio_blk_op(dev->arg,
			start, true,
			pa, n * dev->block_size);
}

int
main(void)
{
	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];
	char dev_name[MESSAGE_LEN];
	struct device dev;

	size_t regs_pa, regs_len;
	size_t regs_va, regs_off;
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
		log(LOG_FATAL, "virtio-blk failed to map registers 0x%x 0x%x!",
				regs_pa, regs_len);
		exit(ERR);
	}

	log(LOG_INFO, "virtio-blk on 0x%x 0x%x with irq %i",
			regs_pa, regs_len, irqn);


	dev.dev = (struct virtio_device *) ((size_t) regs_va + regs_off);

	log(LOG_INFO, "virtio-blk mapped 0x%x -> 0x%x",
			regs_pa, dev.dev);

	if (dev.dev->magic != VIRTIO_DEV_MAGIC) {
		log(LOG_FATAL, "virtio register magic bad 0x%x != expected 0x%x",
				dev.dev->magic, VIRTIO_DEV_MAGIC);
		exit(ERR);
	}

	log(LOG_INFO, "virtio magic = 0x%x",
			dev.dev->magic);

	log(LOG_INFO, "virtio version = 0x%x",
			dev.dev->version);

	if (dev.dev->device_id != VIRTIO_DEV_TYPE_BLK) {
		log(LOG_FATAL, "virtio type bad %i", dev.dev->device_id);
		exit(ERR);
	}

	dev.dev->status = VIRTIO_ACK;
	dev.dev->status |= VIRTIO_DRIVER;

	/* TODO: negotiate */
	dev.dev->device_features_sel = 0;
	dev.dev->driver_features_sel = 0;
	dev.dev->driver_features = dev.dev->device_features;

	dev.dev->device_features_sel = 1;
	dev.dev->driver_features_sel = 1;
	dev.dev->driver_features = dev.dev->device_features;
	
	struct virtio_blk_config *config = 
		(struct virtio_blk_config *) dev.dev->config;

	log(LOG_INFO, "blk cap = 0x%x blocks", config->capacity);
	log(LOG_INFO, "blk size = 0x%x", config->blk_size);

	dev.dev->status |= VIRTIO_FEATURES_OK;
	if (!(dev.dev->status & VIRTIO_FEATURES_OK)) {
		log(LOG_FATAL, "virtio feature set rejected!");
		exit(ERR);
	}

	if (config->blk_size != 512) {
		log(LOG_FATAL, "unsupported block size 0x%x", 
				config->blk_size);
		exit(ERR);
	}

	dev.dev->page_size = PAGE_SIZE;

	if (!virtq_init(&dev.q, dev.dev, 0, 16)) {
		log(LOG_FATAL, "virtio queue init failed");
		exit(ERR);
	}

	dev.dev->status |= VIRTIO_DRIVER_OK;

	if (dev.dev->status != 0xf) {
		log(LOG_FATAL, "virtio status bad");
		exit(ERR);
	}

	union proc0_req rq;
	union proc0_rsp rp;
	rq.irq_reg.type = PROC0_irq_reg_req;
	rq.irq_reg.irqn = irqn; 
	rq.irq_reg.func = &intr_handler;
	rq.irq_reg.arg = &dev;
	rq.irq_reg.sp = &intr_stack[sizeof(intr_stack)];

	if (mesg(PROC0_PID, &rq, &rp) != OK || rp.irq_reg.ret != OK) {
		log(LOG_FATAL, "failed to register interrupt %i : %i", 
				irqn, rp.irq_reg.ret);
		exit(ERR);
	}
	
	dev.buffer_len = PAGE_SIZE;
	dev.buffer_pa = request_memory(dev.buffer_len);
	if (dev.buffer_pa == nil) {
		log(LOG_FATAL, "memory alloc failed");
		exit(ERR);
	}

	dev.buffer_va = map_addr(dev.buffer_pa,
			dev.buffer_len,
			MAP_DEV|MAP_RW);

	if (dev.buffer_va == nil) {
		log(LOG_FATAL, "memory alloc failed");
		exit(ERR);
	}

	log(LOG_INFO, "mapped buffer from 0x%x to 0x%x",
			dev.buffer_pa, dev.buffer_va);

	struct block_dev blk;

	blk.arg = &dev;
	blk.block_size = config->blk_size;
	blk.nblocks = config->capacity;
	blk.name = dev_name;
	
	blk.map_buffers = false;
	blk.read_blocks = &read_blocks;
	blk.write_blocks = &write_blocks;

	return block_dev_register(&blk);
}

