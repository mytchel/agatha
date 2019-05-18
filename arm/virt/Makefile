all: build_libs boot.elf boot.list kernel.list ../proc0/proc0.list

BASE = ${PWD}/../..
ARCH = arm
BOARD = virt

include ../../rules.mk

LOAD_ADDR = 0x40000000

PROCS = \
				../../procs/dev_reg \
				../../procs/log \
				../../procs/init \
				../../procs/video

DRIVERS = \
					../vexpress-a9/drivers/serial-pl01x

BOARD_KERNEL_SRC_C = \
										 cortex_a15_gic.c \
										 ../vexpress-a9/serial_pl01x.c \
										 kernel.c

include ../Makefile

