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


