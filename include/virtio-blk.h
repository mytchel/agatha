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

