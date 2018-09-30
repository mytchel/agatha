all: build_libs kern.umg  kern.list ../proc0/proc0.list

BASE = ${PWD}/../..
ARCH = arm
BOARD = vexpress-a9
MKIMAGE = ~/src/u-boot/tools/mkimage

include ../../rules.mk

LOAD_ADDR = 0x62008000

BUNDLE += ../../procs/dev_reg
BUNDLE += ../drivers/serial-pl01x
BUNDLE += ../drivers/sysreg-sp810
BUNDLE += ../drivers/mmc-pl18x
BUNDLE += ../../procs/init
BUNDLE += ../../procs/fatfs

BOARD_KERNEL_SRC_C = \
	trap.c             \
	serial.c

include ../Makefile

