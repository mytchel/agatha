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
};

#define VIRTIO_BLK_F_SIZE_MAX (1)
/* Maximum size of any single segment is in size_max. */
#define VIRTIO_BLK_F_SEG_MAX (2)
/* Maximum number of segments in a request is in seg_max. */
#define VIRTIO_BLK_F_GEOMETRY (4)
/* Disk-style geometry specified in geometry. */
#define VIRTIO_BLK_F_RO (5)
/* Device is read-only. */
#define VIRTIO_BLK_F_BLK_SIZE (6)
/* Block size of disk is in blk_size. */
#define VIRTIO_BLK_F_FLUSH (9)
/* Cache flush command support. */
#define VIRTIO_BLK_F_TOPOLOGY (10)
/* Device exports information on optimal I/O alignment. */
#define VIRTIO_BLK_F_CONFIG_WCE (11)
/* Device can toggle its cache between writeback and writethrough modes.*/

struct virtio_blk_config {
	uint64_t capacity;
	uint32_t size_max;
	uint32_t seg_max;
	struct {
		uint16_t cylinders;
		uint8_t heads;
		uint8_t sectors;
	} geometry;

	uint32_t blk_size;

	struct {
		uint8_t physical_block_exp;
		uint8_t alignment_offset;
		uint16_t min_io_size;
		uint32_t opt_io_size;
	} topology;
	uint8_t writeback;
};

struct virtio_blk_req {

#define VIRTIO_BLK_T_IN           0 
#define VIRTIO_BLK_T_OUT          1 
#define VIRTIO_BLK_T_FLUSH        4 

	uint32_t type;
	uint32_t reserved;
	uint64_t sector;

	uint8_t data[512];

#define VIRTIO_BLK_S_OK        0 
#define VIRTIO_BLK_S_IOERR     1 
#define VIRTIO_BLK_S_UNSUPP    2

	uint8_t status;
};

#define VIRTIO_NET_F_CSUM (0)
/* Device handles packets with partial checksum. This checksum offload is a common feature on modern network cards. */
#define VIRTIO_NET_F_GUEST_CSUM (1)
/* Driver handles packets with partial checksum. */
#define VIRTIO_NET_F_CTRL_GUEST_OFFLOADS (2)
/* Control channel offloads reconfiguration support. */
#define VIRTIO_NET_F_MAC (5)
/* Device has given MAC address. */
#define VIRTIO_NET_F_GUEST_TSO4 (7)
/* Driver can receive TSOv4. */
#define VIRTIO_NET_F_GUEST_TSO6 (8)
/* Driver can receive TSOv6. */
#define VIRTIO_NET_F_GUEST_ECN (9)
/* Driver can receive TSO with ECN. */
#define VIRTIO_NET_F_GUEST_UFO (10)
/* Driver can receive UFO. */
#define VIRTIO_NET_F_HOST_TSO4 (11)
/* Device can receive TSOv4. */
#define VIRTIO_NET_F_HOST_TSO6 (12)
/* Device can receive TSOv6. */
#define VIRTIO_NET_F_HOST_ECN (13)
/* Device can receive TSO with ECN. */
#define VIRTIO_NET_F_HOST_UFO (14)
/* Device can receive UFO. */
#define VIRTIO_NET_F_MRG_RXBUF (15)
/* Driver can merge receive buffers. */
#define VIRTIO_NET_F_STATUS (16)
/* Configuration status field is available. */
#define VIRTIO_NET_F_CTRL_VQ (17)
/* Control channel is available. */
#define VIRTIO_NET_F_CTRL_RX (18)
/* Control channel RX mode support. */
#define VIRTIO_NET_F_CTRL_VLAN (19)
/* Control channel VLAN filtering. */
#define VIRTIO_NET_F_GUEST_ANNOUNCE (21)
/* Driver can send gratuitous packets. */
#define VIRTIO_NET_F_MQ (22)
/* Device supports multiqueue with automatic receive steering. */
#define VIRTIO_NET_F_CTRL_MAC_ADDR (23)
/* Set MAC address through control channel.*/

struct virtio_net_config {
	uint8_t mac[6];
	
#define VIRTIO_NET_S_LINK_UP     1 
#define VIRTIO_NET_S_ANNOUNCE    2 

	uint16_t status;

	uint16_t max_virtqueue_pairs;
};

struct virtio_net_hdr {
#define VIRTIO_NET_HDR_F_NEEDS_CSUM    1 
	uint8_t flags; 
#define VIRTIO_NET_HDR_GSO_NONE        0 
#define VIRTIO_NET_HDR_GSO_TCPV4       1 
#define VIRTIO_NET_HDR_GSO_UDP         3 
#define VIRTIO_NET_HDR_GSO_TCPV6       4 
#define VIRTIO_NET_HDR_GSO_ECN      0x80 
	uint8_t gso_type; 
	uint16_t hdr_len; 
	uint16_t gso_size; 
	uint16_t csum_start; 
	uint16_t csum_offset; 
}__attribute__((packed));

