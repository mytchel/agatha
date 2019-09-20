all: boot.elf boot.list kernel.list 

ARCH = arm
BOARD = virt

LOAD_ADDR = 0x40000000

PROCS = \
	procs/dev_reg \
	procs/log \
	procs/net_test \
	procs/init \
	procs/ui 

IDLE = \
	procs/idle 

DRIVERS = \
	arm/drivers/serial-pl01x \
	arm/drivers/virtio/net \
	arm/drivers/virtio/gpu \
	arm/drivers/virtio/input \
	arm/drivers/virtio/blk 

BOARD_KERNEL_SRC_A = \
	arm/virt/timer.S

BOARD_KERNEL_SRC_C = \
	arm/virt/cortex_a15_gic.c \
	arm/virt/generic_timer.c \
	arm/vexpress-a9/serial_pl01x.c \
	arm/virt/kernel.c

CFLAGS  += -DSYSTICK=500
LDFLAGS += 

include arm/Makefile

