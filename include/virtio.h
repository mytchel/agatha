#define VIRTIO_DEV_MAGIC  0x74726976

#define VIRTIO_DEV_TYPE_INVALID   0
#define VIRTIO_DEV_TYPE_NET       1
#define VIRTIO_DEV_TYPE_BLK       2
#define VIRTIO_DEV_TYPE_GPU      16
#define VIRTIO_DEV_TYPE_INPUT    18

#define VIRTIO_ACK           1
#define VIRTIO_DRIVER        2
#define VIRTIO_FAILED      128
#define VIRTIO_FEATURES_OK   8
#define VIRTIO_DRIVER_OK     4
#define VIRTIO_NEED_RESET   64

/*
	 New version 2 which qemu doesn't use.
	 or my version doesn't use 
 */

struct virtio2_device {
	uint32_t magic;
	uint32_t version;
	uint32_t device_id;
	uint32_t vendor_id;
	uint32_t device_features;
	uint32_t device_features_sel;
	uint32_t pad0[2];
	uint32_t driver_features;
	uint32_t driver_features_sel;
	uint32_t pad1[2];
	uint32_t queue_sel;
	uint32_t queue_num_max;
	uint32_t queue_num;
	uint32_t pad2[2];
	uint32_t queue_ready;
	uint32_t pad3[2];
	uint32_t queue_notify;
	uint32_t pad4[3];
	uint32_t interrupt_status;
	uint32_t interrupt_ack;
	uint32_t pad5[2];
	uint32_t status;
	uint32_t pad6[3];
	uint32_t queue_desc_low;
	uint32_t queue_desc_high;
	uint32_t pad7[2];
	uint32_t queue_avail_low;
	uint32_t queue_avail_high;
	uint32_t pad8[2];
	uint32_t queue_used_low;
	uint32_t queue_used_high;
	uint32_t pad9[21];
	uint32_t config_generation;
	uint8_t config[256];
};

struct virtio_device {
	uint32_t magic;
	uint32_t version;
	uint32_t device_id;
	uint32_t vendor_id;
	uint32_t device_features;
	uint32_t device_features_sel;
	uint32_t pad0[2];
	uint32_t driver_features;
	uint32_t driver_features_sel;
	uint32_t page_size;
	uint32_t pad1[1];
	uint32_t queue_sel;
	uint32_t queue_num_max;
	uint32_t queue_num;
	uint32_t queue_used_align;
	uint32_t queue_pfn;
	uint32_t pad2[3];
	uint32_t queue_notify;
	uint32_t pad4[3];
	uint32_t interrupt_status;
	uint32_t interrupt_ack;
	uint32_t pad5[2];
	uint32_t status;
	uint32_t pad6[35];
	uint8_t config[256];
};

struct virtq_desc {
	uint64_t addr;
	uint32_t len;
#define VIRTQ_DESC_F_NEXT      1
#define VIRTQ_DESC_F_WRITE     2
#define VIRTQ_DESC_F_INDIRECT  4
	uint16_t flags;
	uint16_t next;
};

struct virtq_avail {
#define VIRTQ_AVAIL_F_NO_INTERRUPT  1
	uint16_t flags;
	uint16_t idx;
	uint16_t rings[];
	/*
		 Only used if VIRTIO_F_EVENT_IDX
	uint16_t used_event;
	*/
};

struct virtq_used_item {
	uint32_t index;
	uint32_t len;
};

struct virtq_used {
	uint16_t flags;
	uint16_t index;
	struct virtq_used_item rings[];
	/*
		 Only used if VIRTIO_F_EVENT_IDX
	uint16_t avail_event;
	*/
};

struct virtq {
	volatile struct virtio_device *dev;
	size_t queue_index;
	size_t size;
	size_t last_seen_used;
	struct virtq_desc *desc;
	struct virtq_avail *avail;
	struct virtq_used *used;

	size_t desc_free_index;	
};

size_t 
virtq_size(size_t qsz);

struct virtq_desc *
virtq_get_desc(struct virtq *q, size_t *index);

void
virtq_push(struct virtq *q, size_t index);

struct virtq_used_item *
virtq_pop(struct virtq *q);

void
virtq_free_desc(struct virtq *q, 
		struct virtq_desc *d, size_t index);

bool
virtq_init(struct virtq *q, 
		volatile struct virtio_device *dev, 
		size_t queue_index,
		size_t num);

