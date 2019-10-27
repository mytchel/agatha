typedef enum {
	PROC0_get_resource,
	PROC0_mem_req,

	DEV_REG_find,
	DEV_REG_register,
	DEV_REG_list,

	LOG_register,
	LOG_log,

	BLOCK_info,
	BLOCK_read,
	BLOCK_write,

	SERIAL_configure,
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
	VIDEO_create_frame,
	VIDEO_draw,

	EVDEV_info,
	EVDEV_connect,
	EVDEV_event,
	NET_bind,
	NET_unbind,

	NET_tcp_connect,
	NET_tcp_disconnect,
	NET_tcp_listen,

	NET_write,
	NET_read,

	TIMER_create,
	TIMER_set,

} mesg_type_t;

