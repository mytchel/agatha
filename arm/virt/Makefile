all: kern.umg kern.list

BASE = ${PWD}/../..
include ../../port_rules.mk
include ../rules.mk
include rules.mk

LOAD_ADDR = 0x62008000

BUNDLE += $(BASE)/drivers/ping
BUNDLE += $(BASE)/drivers/pong

KERNEL_SRC_A = 

KERNEL_SRC_C = \
	kern/dev.c       \
	kern/trap.c      \
	kern/serial.c

include ../Makefile

