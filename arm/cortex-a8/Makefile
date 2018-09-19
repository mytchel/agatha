all: build_libs kern.umg kern.list ../proc0/proc0.list

BASE = ${PWD}/../..
ARCH = arm
CPU = cortex-a8

include ../../rules.mk

LOAD_ADDR = 0x82000000

BUNDLE += ../../drivers/ping
BUNDLE += ../../drivers/pong

KERNEL_SRC_A += 

KERNEL_SRC_C += \
	trap.c        \
	serial.c      \
	systick.c

include ../Makefile

