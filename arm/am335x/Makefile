all: kern.umg kern.list

BASE = ${PWD}/../..
ARCH = arm
BOARD = am335x

include ../../rules.mk

LOAD_ADDR = 0x82000000

BUNDLE += ../../drivers/ping
BUNDLE += ../../drivers/pong

KERNEL_SRC_A = 

KERNEL_SRC_C = \
	trap.c   \
	serial.c    \
	systick.c

include ../Makefile

