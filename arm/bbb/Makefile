all: kern.umg kern.list

BASE = ${.CURDIR}/../..
ARCH = arm
BOARD = bbb

include ../../rules.mk

LOAD_ADDR = 0x82000000

BUNDLE += ../../drivers/ping
BUNDLE += ../../drivers/pong

KERNEL_SRC_A = 

KERNEL_SRC_C = \
	kern/dev.c           \
	kern/am33xx-trap.c   \
	kern/omap3-uart.c    \
	kern/am335x-timer.c

include ../Makefile

