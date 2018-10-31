all: build_libs kern.umg  kern.list ../proc0/proc0.list

BASE = ${PWD}/../..
ARCH = arm
BOARD = odroid-c1
MKIMAGE = ~/src/u-boot-odroid/build/tools/mkimage

include ../../rules.mk

LOAD_ADDR = 0x13000000

BUNDLE += ../../procs/dev_reg
BUNDLE += ../../procs/init

BOARD_KERNEL_SRC_C =  \
	../kern/gic.c       \
	serial_aml_uart.c   \
	drivers.c

include ../Makefile

