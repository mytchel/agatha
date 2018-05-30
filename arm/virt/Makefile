all: kern.umg 

BASE = ${PWD}/../..
ARCH = arm
BOARD = virt

include ../../rules.mk

LOAD_ADDR = 0x62008000

BUNDLE += ../../drivers/ping
BUNDLE += ../../drivers/pong

KERNEL_SRC_A = 

KERNEL_SRC_C = \
	trap.c      \
	serial.c

include ../Makefile

