all: build_libs kern.umg  kern.list ../proc0/proc0.list

BASE = ${PWD}/../..
ARCH = arm
BOARD = vexpress-a9
MKIMAGE = ~/src/u-boot/tools/mkimage

include ../../rules.mk

LOAD_ADDR = 0x62008000

PROCS = \
				../../procs/dev_reg \
				../../procs/log \
				../../procs/init

DRIVERS = \
					drivers/serial-pl01x \
					drivers/sysreg-sp810 \
					drivers/mmc-pl18x    \
					drivers/ethernet-lan9118

BOARD_KERNEL_SRC_C = \
										 ../kern/gic.c      \
										 serial_pl01x.c     \
										 kernel.c

include ../Makefile

