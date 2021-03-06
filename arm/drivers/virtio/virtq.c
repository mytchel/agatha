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
#include <virtio.h>

size_t virtq_size(size_t qsz)
{
	return PAGE_ALIGN(sizeof(struct virtq_desc) * qsz + sizeof(uint16_t)*(3+qsz)) 
		+ PAGE_ALIGN(sizeof(uint16_t)*3 + sizeof(struct virtq_used_item)*qsz);
}

struct virtq_desc *
virtq_get_desc(struct virtq *q, size_t *index)
{
	struct virtq_desc *d;

	if (q->desc_free_index == 0xffff) {
		log(LOG_INFO, "virtq get desc fail");
		return nil;
	}

	*index = q->desc_free_index;
	d = &q->desc[*index];
	q->desc_free_index = d->next;

	return d;
}

void
virtq_free_desc(struct virtq *q, struct virtq_desc *d, size_t index)
{
	d->next = q->desc_free_index;

	q->desc_free_index = index;
}

void
virtq_push(struct virtq *q, size_t index)
{
	size_t idx;
 
	idx = q->avail->idx % q->size;
	q->avail->rings[idx] = index;
	q->avail->idx++;

	q->dev->queue_notify = q->queue_index;
}

struct virtq_used_item *
virtq_pop(struct virtq *q)
{
	struct virtq_used_item *e;

	if (q->last_seen_used == q->used->index) {
		return nil;
	}

	e = &q->used->rings[q->last_seen_used % q->size];
	q->last_seen_used++;

	q->dev->interrupt_ack = 1;

	return e;
}

bool
virtq_init(struct virtq *q, 
		volatile struct virtio_device *dev, 
		size_t queue_index,
		size_t num)
{
	size_t queue_pa, queue_va, queue_len;
	size_t avail_off, used_off;
	size_t i, cid;
	int type;

	q->queue_index = queue_index;

	dev->queue_sel = queue_index;
	
	if (dev->queue_num_max < num) {
		log(LOG_FATAL, "queue len %i is too large for queue %i (max = %i)",
				num, queue_index, dev->queue_num_max);
		return false;
	}

	queue_len = PAGE_ALIGN(virtq_size(num));

	log(LOG_INFO, "need 0x%x bytes for queue of size 0x%x", 
			queue_len, num);

	cid = request_memory(queue_len, 0x1000);
	if (cid < 0) {
		log(LOG_FATAL, "virtio queue memory alloc failed");
		return false;
	}

	frame_info(cid, &type, &queue_pa, &queue_len);

	queue_va = (size_t) frame_map_anywhere(cid);
	if (queue_va == nil) {
		log(LOG_FATAL, "virtio queue memory map failed");
		release_memory(cid);
		return false;
	}

	memset((void *) queue_va, 0, queue_len);

	avail_off = sizeof(struct virtq_desc) * num;
	used_off = PAGE_ALIGN(sizeof(struct virtq_desc) * num + 
			sizeof(uint16_t)*(3+num));

	q->last_seen_used = 0;
	q->size = num;
	q->dev = dev;
	q->desc = (void *) queue_va;
	q->avail = (void *) (queue_va + avail_off);
	q->used = (void *) (queue_va + used_off);

	dev->queue_num = num;
	dev->queue_used_align = PAGE_SIZE;
	dev->queue_pfn = queue_pa >> PAGE_SHIFT;

	q->desc_free_index = 0;
	
	for (i = 0; i < num-1; i++) {
		q->desc[i].next = i + 1;
	}

	q->desc[num-1].next = 0xffff;

	return true;
}


