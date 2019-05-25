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


