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

size_t virtq_size(size_t qsz)
{
	return PAGE_ALIGN(sizeof(struct virtq_desc) * qsz + sizeof(uint16_t)*(3+qsz)) 
		+ PAGE_ALIGN(sizeof(uint16_t)*3 + sizeof(struct virtq_used_item)*qsz);
}

	void
main(void)
{
	uint32_t init_m[MESSAGE_LEN/sizeof(uint32_t)];
	char dev_name[MESSAGE_LEN];

	size_t regs_pa, regs_len;
	size_t regs_va, regs_off;
	struct virtio_device *dev;

	recv(0, init_m);
	regs_pa = init_m[0];
	regs_len = init_m[1];

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

	dev = (struct virtio_device *) ((size_t) regs_va + regs_off);

	log(LOG_INFO, "virtio-blk mapped 0x%x -> 0x%x",
			regs_pa, dev);

	if (dev->magic != VIRTIO_DEV_MAGIC) {
		log(LOG_FATAL, "virtio register magic bad 0x%x != expected 0x%x",
				dev->magic, VIRTIO_DEV_MAGIC);
		exit(ERR);
	}

	log(LOG_INFO, "virtio magic = 0x%x",
			dev->magic);

	log(LOG_INFO, "virtio version = 0x%x",
			dev->version);

	if (dev->device_id != VIRTIO_DEV_TYPE_BLK) {
		log(LOG_FATAL, "virtio type bad %i", dev->device_id);
		exit(ERR);
	}

	dev->status = VIRTIO_ACK;
	dev->status |= VIRTIO_DRIVER;

	/* TODO: negotiate */
	dev->device_features_sel = 0;
	log(LOG_INFO, "features 0 = 0x%x", dev->device_features);
	dev->driver_features_sel = 0;
	dev->driver_features = dev->device_features;

	dev->device_features_sel = 1;
	log(LOG_INFO, "features 1 = 0x%x", dev->device_features);
	dev->driver_features_sel = 1;
	dev->driver_features = dev->device_features;
	
	struct virtio_blk_config *config = 
		(struct virtio_blk_config *) dev->config;

	log(LOG_INFO, "blk cap = 0x%x blocks", config->capacity);
	log(LOG_INFO, "blk size = 0x%x", config->blk_size);

	dev->status |= VIRTIO_FEATURES_OK;
	if (!(dev->status & VIRTIO_FEATURES_OK)) {
		log(LOG_FATAL, "virtio feature set rejected!");
		exit(ERR);
	}

	dev->page_size = PAGE_SIZE;

	/* Setup queues, ect */

	dev->queue_sel = 0;
	
	log(LOG_INFO, "queue 0 max size = 0x%x", dev->queue_num_max);

	size_t ql = 16;

	size_t queue_len = PAGE_ALIGN(virtq_size(ql));
	log(LOG_INFO, "need 0x%x bytes for queue of size 0x%x", queue_len, ql);

	size_t queue_pa = request_memory(queue_len);
	if (queue_pa == nil) {
		log(LOG_FATAL, "virtio queue memory alloc failed");
		exit(ERR);
	}

	uint8_t *queue_va = map_addr(queue_pa,
			queue_len,
			MAP_DEV|MAP_RW);

	memset(queue_va, 0, queue_len);

	struct virtq q;

	log(LOG_INFO, "sizeof desc = 0x%x", sizeof(struct virtq_desc));
	size_t avail_off = sizeof(struct virtq_desc) * ql;
	size_t used_off = PAGE_ALIGN(sizeof(struct virtq_desc) * ql + sizeof(uint16_t)*(3+ql));

	log(LOG_INFO, "avail offset = 0x%x", avail_off);
	log(LOG_INFO, "used offset = 0x%x", used_off);

	q.desc = (void *) queue_va;
	q.avail = (void *) &queue_va[avail_off];
	q.used = (void *) &queue_va[used_off];

	log(LOG_INFO, "status = 0x%x", dev->status);

	dev->queue_num = ql;
	dev->queue_used_align = PAGE_SIZE;
	dev->queue_pfn = queue_pa >> PAGE_SHIFT;

	log(LOG_INFO, "status = 0x%x", dev->status);

	yield();

	dev->status |= VIRTIO_DRIVER_OK;

	log(LOG_INFO, "status = 0x%x", dev->status);

	size_t test_pa = request_memory(0x1000);
	if (test_pa == nil) {
		log(LOG_FATAL, "virtio queue memory alloc failed");
		exit(ERR);
	}

	uint8_t *test_va = map_addr(test_pa,
			0x1000,
			MAP_DEV|MAP_RW);

	log(LOG_INFO, "mapped test mem from 0x%x to 0x%x",
			test_pa, test_va);

	struct virtq_desc *d;

	d = q.desc;

	log(LOG_INFO, "status = 0x%x", dev->status);
	log(LOG_INFO, "set up buffers");

	d[0].addr = test_pa;
	d[0].len = 4+4+8;
	d[0].next = 1;
	d[0].flags = VIRTQ_DESC_F_NEXT;

	d[1].addr = test_pa + 512;
	d[1].len = 512;
	d[1].next = 2;
	d[1].flags = VIRTQ_DESC_F_WRITE|VIRTQ_DESC_F_NEXT;
	
	d[2].addr = test_pa + 1024;	
	d[2].len = 1;
	d[2].next = 0;
	d[2].flags = VIRTQ_DESC_F_WRITE;

	struct virtio_blk_req *r = (void *) test_va;
	r->type = VIRTIO_BLK_T_IN;
	r->reserved = 0;
	r->sector = 0;

	size_t idx = q.avail->idx % 16;
	
	q.avail->rings[idx] = 0;
	
	q.avail->idx += 3; /* Or add 3 for the 3 buffers? */

	dev->queue_notify = 0;
	log(LOG_INFO, "status = 0x%x", dev->status);
	log(LOG_INFO, "now wait for interrupt 0x%x", &dev->interrupt_status);

	while (dev->interrupt_status == 0)
		;

	log(LOG_INFO, "got interrupt 0x%x", dev->interrupt_status);
	dev->interrupt_ack = 1;

	log(LOG_INFO, "status = 0x%x", test_va[1024]);

	int i;
	for (i = 0; i < 512; i++) {
		log(LOG_INFO, "0x%x : %x", i, test_va[512 + i]);
	}
	
}

