all: build_libs kern.elf  kern.list ../proc0/proc0.list

BASE = ${PWD}/../..
ARCH = arm
BOARD = odroid-c1
MKIMAGE = ~/src/u-boot-odroid/build/tools/mkimage

include ../../rules.mk

LOAD_ADDR = 0x30000000

BUNDLE += ../../procs/dev_reg
#BUNDLE += ../../procs/init
#BUNDLE += ../../procs/fatfs

BOARD_KERNEL_SRC_C = \
	trap.c             \
	serial.c

include ../Makefile

