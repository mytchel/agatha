all: build_libs kern.umg kern.list ../proc0/proc0.list

BASE = ${PWD}/../..
ARCH = arm
BOARD = am335x

include ../../rules.mk

LOAD_ADDR = 0x82000000

#BUNDLE += ../drivers/mmc-omap3
BUNDLE += ../drivers/serial-omap3

KERNEL_SRC_A += 

KERNEL_SRC_C += \
	trap.c        \
	serial.c      \
	systick.c

include ../Makefile

