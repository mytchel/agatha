ARCH = arm
CROSS = arm-none-eabi

CC = $(CROSS)-gcc
LD = $(CROSS)-ld
AR = $(CROSS)-ar
OBJCOPY = $(CROSS)-objcopy
OBJDUMP = $(CROSS)-objdump
MKUBOOT = $(BASE)/mkuboot

USER.LD = $(BASE)/arm/user.ld
USER_ADDR = 0x00010000

CFLAGS += \
	-fno-stack-protector \
	-DUSER_ADDR=$(USER_ADDR) \
	-DLOAD_ADDR=$(LOAD_ADDR) \
	-I$(BASE)/arm/include
	
LDFLAGS += \
	-L/usr/local/lib/gcc/$(CROSS)/6.3.1


KERNEL_SRC_A := \
        $(BASE)/arm/kern/vectors.S     \
        $(BASE)/arm/kern/asm.S

KERNEL_SRC_C := \
        $(BASE)/arm/kern/main.c       \
        $(BASE)/arm/kern/mem.c        \
        $(BASE)/arm/kern/proc.c       \
        $(BASE)/arm/kern/intr.c       \
        $(BASE)/kern/debug.c          \
        $(BASE)/kern/proc.c           \
        $(BASE)/kern/sys.c            \
        $(BASE)/kern/util.c           \
        $(BASE)/lib/libstring/conv.c  \
        $(BASE)/lib/libstring/scanf.c \
        $(BASE)/lib/libstring/string.c\
        $(BASE)/lib/libfdt/fdt.c

