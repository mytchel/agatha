USER.LD = arm/user.ld
USER_ADDR = 0x00010000

CFLAGS += \
	-fno-stack-protector \
	-mcpu=cortex-a8 \
	-Iarm/include

LDFLAGS +=

ARM_KERNEL_SRC_A := \
        arm/kern/vectors.S            \
        arm/kern/asm.S

ARM_KERNEL_SRC_C := \
        arm/kern/main.c               \
        arm/kern/mem.c                \
        arm/kern/proc.c               \
        arm/kern/intr.c               \
        kern/debug.c                  \
        kern/proc.c                   \
        kern/sys.c                    \
        kern/util.c                   \
        lib/libstring/conv.c          \
        lib/libstring/scanf.c         \
        lib/libstring/string.c        \
        lib/libfdt/fdt.c

PROC0 = arm/proc0

include $(PROC0)/Makefile
CLEAN += $(PROC0)/proc0.bin $(PROC0)/proc0.bo

include arm/bbb/Makefile
include arm/virt/Makefile

