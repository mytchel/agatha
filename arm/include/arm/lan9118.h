struct lan9118_regs {
	uint32_t rx_data_fifo_port;
	uint32_t rx_fifo_alias_ports[7];
	uint32_t tx_data_fifo_port;
	uint32_t tx_fifo_alias_ports[7];
	uint32_t rx_stat_fifo_port;
	uint32_t rx_stat_fifo_peek;
	uint32_t tx_stat_fifo_port;
	uint32_t tx_stat_fifo_peek;

	uint32_t id_rev;
	uint32_t irq_cfg;
	uint32_t int_sts;
	uint32_t int_en;
	uint32_t res0;
	uint32_t byte_test;
	uint32_t fifo_int;
	uint32_t rx_cfg;
	uint32_t tx_cfg;
	uint32_t hw_cfg;
	uint32_t rx_dp_ctl;
	uint32_t rx_fifo_inf;
	uint32_t tx_fifo_inf;
	uint32_t pmt_ctrl;
	uint32_t gpio_cfg;
	uint32_t gpt_cfg;
	uint32_t gpt_cnt;
	uint32_t res1;
	uint32_t word_swap;
	uint32_t free_run;
	uint32_t rx_drop;
	uint32_t mac_csr_cmd;
	uint32_t mac_csr_data;
	uint32_t afc_cfg;
	uint32_t e2p_cmd;
	uint32_t e2p_data;
};

