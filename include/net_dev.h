struct net_dev {
	void *arg;

	char name[16];

	int mount_eid;

	uint16_t ipv4_ident;

	size_t mtu;

	uint8_t mac[6];
	uint8_t ipv4[4];

	void (*send_pkt)(struct net_dev *dev,
			uint8_t *buf, size_t len);

	void *internal;
};

void
net_process_pkt(struct net_dev *dev,
		uint8_t *buf, size_t len);

void
net_handle_message(struct net_dev *dev,
		int eid, int from, uint8_t *m, int cap);

int
net_init(struct net_dev *dev);

