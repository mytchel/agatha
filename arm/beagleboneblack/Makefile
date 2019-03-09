all: build_libs kern.umg  kern.list ../proc0/proc0.list

BASE = ${PWD}/../..
ARCH = arm
BOARD = beagleboneblack
MKIMAGE = ~/src/u-boot/tools/mkimage

include ../../rules.mk

LOAD_ADDR = 0x82000000

PROCS = \
	../../procs/dev_reg \
	../../procs/log \
	../../procs/init

DRIVERS = \
					drivers/serial-omap3 \
					drivers/i2c-am335x \
					drivers/prm-cm-am33xx \
					drivers/mmc-omap3 \
					drivers/video-am335x

BOARD_KERNEL_SRC_C =  \
	cortex_a8_intr.c      \
	serial_omap3_uart.c   \
	am335x_dmtimer.c      \
	kernel.c

include ../Makefile

