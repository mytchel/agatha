size_t 
virtq_size(size_t qsz);

	struct virtq_desc *
virtq_get_desc(struct virtq *q, size_t *index);

	void
virtq_push(struct virtq *q, size_t index);

struct virtq_used_item *
virtq_pop(struct virtq *q);

	bool
virtq_init(struct virtq *q, 
		volatile struct virtio_device *dev, 
		size_t queue_index,
		size_t num);

