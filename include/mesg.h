typedef enum {
	PROC0_irq_reg_req,
	PROC0_irq_reg_rsp,
	PROC0_proc_req,
	PROC0_proc_rsp,
	PROC0_addr_req_req,
	PROC0_addr_req_rsp,
	PROC0_addr_map_req,
	PROC0_addr_map_rsp,
	PROC0_addr_give_req,
	PROC0_addr_give_rsp,

	PROC_start_msg,
	PROC_fault_msg,
	PROC_exit_msg,

	DEV_REG_find_req,
	DEV_REG_find_rsp,
	DEV_REG_register_req,
	DEV_REG_register_rsp,
	DEV_REG_list_req,
	DEV_REG_list_rsp,

	LOG_register_req,
	LOG_register_rsp,
	LOG_log_req,
	LOG_log_rsp,

	BLOCK_info_req,
	BLOCK_info_rsp,
	BLOCK_read_req,
	BLOCK_read_rsp,
	BLOCK_write_req,
	BLOCK_write_rsp,

	SERIAL_configure_req,
	SERIAL_configure_rsp,
	SERIAL_read_req,
	SERIAL_read_rsp,
	SERIAL_write_req,
	SERIAL_write_rsp,

	I2C_configure_req,
	I2C_configure_rsp,
	I2C_read_req,
	I2C_read_rsp,
	I2C_write_req,
	I2C_write_rsp,

	FILE_start_req,
	FILE_start_rsp,
	FILE_find_req,
	FILE_find_rsp,
	FILE_stat_req,
	FILE_stat_rsp,
	FILE_read_req,
	FILE_read_rsp,
	FILE_write_req,
	FILE_write_rsp,
	FILE_clunk_req,
	FILE_clunk_rsp,
	
	VIDEO_connect_req,
	VIDEO_connect_rsp,
	VIDEO_update_req,
	VIDEO_update_rsp,

	EVDEV_info_req,
	EVDEV_info_rsp,
	EVDEV_connect_req,
	EVDEV_connect_rsp,
	EVDEV_event_msg,

} mesg_type_t;

