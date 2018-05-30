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
	dev.c           \
	am33xx-trap.c   \
	omap3-uart.c    \
	am335x-timer.c

include ../Makefile

