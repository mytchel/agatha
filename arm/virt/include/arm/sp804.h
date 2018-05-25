struct sp804_regs {
	struct {
		uint32_t load;
		uint32_t value;
		uint32_t control;
		uint32_t intclr;
		uint32_t ris;
		uint32_t mis;
		uint32_t bgload;
		uint32_t reserved;
	} t[2];

	/* There are other registers here. */
};

#define SP804_TIMER_ENABLE       (1 << 7)
#define SP804_TIMER_PRESCALE_1   (0 << 2)
#define SP804_TIMER_PRESCALE_16  (1 << 2)
#define SP804_TIMER_PRESCALE_256 (2 << 2)
#define SP804_TIMER_INT_ENABLE   (1 << 5)
#define SP804_TIMER_32BIT        (1 << 1)
#define SP804_TIMER_ONESHOT      (1 << 0)
#define SP804_TIMER_PERIODIC     (1 << 6)

