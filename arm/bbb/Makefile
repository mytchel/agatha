all: kern.umg kern.list

BASE = ${PWD}/../..
ARCH = arm
BOARD = bbb

include ../../rules.mk

LOAD_ADDR = 0x82000000

BUNDLE += ../../drivers/ping
BUNDLE += ../../drivers/pong

KERNEL_SRC_A = 

KERNEL_SRC_C = \
	am33xx-trap.c   \
	omap3-uart.c    \
	am335x-timer.c

include ../Makefile

