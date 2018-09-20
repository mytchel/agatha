all: build_libs kern.umg  kern.list ../proc0/proc0.list

BASE = ${PWD}/../..
ARCH = arm
BOARD = vexpress-a9

include ../../rules.mk

LOAD_ADDR = 0x62008000

#BUNDLE += ../../drivers/ping
#BUNDLE += ../../drivers/pong
BUNDLE += ../drivers/serial-pl01x
#BUNDLE += ../drivers/sysreg-sp810
BUNDLE += ../drivers/mmc-pl18x

KERNEL_SRC_A += 

KERNEL_SRC_C += \
	trap.c       \
	serial.c

include ../Makefile

