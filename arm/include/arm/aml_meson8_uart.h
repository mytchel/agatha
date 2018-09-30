#define UART_STATUS_RECV_BUSY  (1<<26)
#define UART_STATUS_XMIT_BUSY  (1<<25)
#define UART_STATUS_TX_EMPTY   (1<<22)
#define UART_STATUS_TX_FULL    (1<<21)

struct aml_meson8_uart_regs {
	uint32_t wfifo;
	uint32_t rfifo;
	uint32_t control;
	uint32_t status;
	uint32_t misc;
	uint32_t reg5;
};

