all: boot.elf 

ARCH = arm
BOARD = vexpress-a9

LOAD_ADDR = 0x62008000

IDLE = \
	procs/idle 

DRIVERS = \
	arm/drivers/serial-pl01x \
	arm/drivers/virtio/net \
	arm/drivers/timer-sp804 \
	arm/drivers/mmc-pl18x \
#	arm/drivers/video-pl111

PROCS = \
	procs/log \
	procs/test \
	procs/lfs \
#	procs/ui


BOARD_KERNEL_SRC_C = \
	arm/vexpress-a9/cortex_a9_gic_systick.c \
	arm/vexpress-a9/serial_pl01x.c \
	arm/vexpress-a9/kernel.c

CFLAGS  += -DSYSTICK=1000000
#CFLAGS  += -DSYSTICK=100
LDFLAGS += 

include arm/Makefile

