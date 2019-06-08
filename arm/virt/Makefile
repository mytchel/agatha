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

IDLE = \
				../../procs/idle 

DRIVERS = \
					../drivers/serial-pl01x \
					../drivers/virtio/blk \
					../drivers/virtio/net \
					../drivers/virtio/gpu

BOARD_KERNEL_SRC_A = \
										 timer.S

BOARD_KERNEL_SRC_C = \
										 cortex_a15_gic.c \
										 generic_timer.c \
										 ../vexpress-a9/serial_pl01x.c \
										 kernel.c

include ../Makefile

