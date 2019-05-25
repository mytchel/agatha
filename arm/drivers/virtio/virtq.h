size_t 
virtq_size(size_t qsz);

	struct virtq_desc *
virtq_get_desc(struct virtq *q, size_t *index);

	void
virtq_send(struct virtq *q, size_t index);

	bool
virtq_init(struct virtq *q, 
		volatile struct virtio_device *dev, 
		size_t queue_index,
		size_t num);

