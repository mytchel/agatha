all: kern.umg kern.list

BASE = ${PWD}/../..
include ../../port_rules.mk
include ../rules.mk
include rules.mk

LOAD_ADDR = 0x82000000

DEVICE_TABLE = devices.c

BUNDLE += ../../drivers/test

KERNEL_SRC_A = 

KERNEL_SRC_C = \
	kern/dev.c           \
	kern/am33xx-trap.c   \
	kern/omap3-uart.c    \
	kern/am335x-timer.c

include ../Makefile

