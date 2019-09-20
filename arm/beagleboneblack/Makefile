all: boot.elf boot.list kernel.list 

ARCH = arm
BOARD = beagleboneblack

MKIMAGE = ~/src/u-boot/tools/mkimage

LOAD_ADDR = 0x82000000

PROCS = \
	procs/dev_reg \
	procs/log \
	procs/init

IDLE = \
	procs/idle

DRIVERS = \
	arm/drivers/serial-omap3 \
	arm/drivers/prm-cm-am33xx \
	arm/drivers/i2c-am335x \
	arm/drivers/mmc-omap3 \
	arm/drivers/video-am335x

BOARD_KERNEL_SRC_C = \
	arm/beagleboneblack/cortex_a8_intr.c \
	arm/beagleboneblack/serial_omap3_uart.c \
	arm/beagleboneblack/am335x_dmtimer.c \
	arm/beagleboneblack/kernel.c

CFLAGS += -DSYSTICK=100000
LDFLAGS +=

include arm/Makefile

