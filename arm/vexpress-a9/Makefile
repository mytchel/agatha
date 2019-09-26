all: boot.elf 

ARCH = arm
BOARD = vexpress-a9

LOAD_ADDR = 0x62008000

IDLE = \
	procs/idle 

DRIVERS = \
	arm/drivers/serial-pl01x \
#	arm/drivers/timer-sp804 \
#	arm/drivers/mmc-pl18x \
#	arm/drivers/virtio/net \
#	arm/drivers/ethernet-lan9118 \
#	arm/drivers/video-pl111

PROCS = \
	procs/log \
	procs/init \
#	procs/net_test \
#	procs/ui


BOARD_KERNEL_SRC_C = \
	arm/vexpress-a9/cortex_a9_gic_systick.c \
	arm/vexpress-a9/serial_pl01x.c \
	arm/vexpress-a9/kernel.c

CFLAGS  += -DSYSTICK=0xfffffff
LDFLAGS += 

include arm/Makefile

