all: build_libs kern.umg boot.list kernel.list ../proc0/proc0.list

BASE = ${PWD}/../..
ARCH = arm
BOARD = vexpress-a9
MKIMAGE = ~/src/u-boot/tools/mkimage

include ../../rules.mk

LOAD_ADDR = 0x62008000

PROCS = \
				../../procs/dev_reg \
				../../procs/log \
				../../procs/init \
				../../procs/video

DRIVERS = \
					drivers/serial-pl01x \
					drivers/mmc-pl18x    \
					drivers/ethernet-lan9118 \
					drivers/video-pl111

BOARD_KERNEL_SRC_C = \
										 ../kern/gic.c      \
										 serial_pl01x.c     \
										 kernel.c

include ../Makefile

