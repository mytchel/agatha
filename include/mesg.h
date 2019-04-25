typedef enum {
	PROC0_irq_reg,
	PROC0_proc,
	PROC0_addr_req,
	PROC0_addr_map,
	PROC0_addr_give,

	DEV_REG_find,
	DEV_REG_register,
	DEV_REG_list,

	LOG_register,
	LOG_log,

	BLOCK_info,
	BLOCK_read,
	BLOCK_write,

	SERIAL_read,
	SERIAL_write,

	I2C_configure,
	I2C_read,
	I2C_write,

	FILE_start,
	FILE_find,
	FILE_stat,
	FILE_read,
	FILE_write,
	FILE_clunk,
	
	VIDEO_connect,
	VIDEO_update,

} mesg_type_t;

