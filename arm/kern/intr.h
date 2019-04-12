struct irq_handler {
	bool registered;
	bool exiting;

	struct intr_mapping map;
	label_t label;

	struct irq_handler *next;
};

extern struct irq_handler *active_irq;

void
irq_disable(size_t irqn);

void
irq_enable(size_t irqn);

void
irq_clear(size_t irqn);

void
irq_end(size_t irqn);

int
irq_enter(struct irq_handler *h);

void
irq_handler(void);

