struct device {
	char name[256];
	char compatable[256];
	size_t reg, len;
	size_t irqn;
};

extern struct device devices[];

