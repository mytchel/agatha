all: kern.umg kern.list

BASE = ${.CURDIR}/../..
ARCH = arm
BOARD = virt

include ../../rules.mk

LOAD_ADDR = 0x62008000

BUNDLE += ../../drivers/ping
BUNDLE += ../../drivers/pong

KERNEL_SRC_A = 

KERNEL_SRC_C = \
	dev.c       \
	trap.c      \
	serial.c

include ../Makefile

