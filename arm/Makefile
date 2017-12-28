CFLAGS += -mcpu=cortex-a8

KERNEL_SRC_A := \
        arm/kern/vectors.S            \
        arm/kern/asm.S

KERNEL_SRC_C := \
        arm/kern/main.c               \
        arm/kern/trap.c               \
        arm/kern/mem.c                \
        arm/kern/proc.c               \
        arm/kern/dev.c                \
        arm/kern/uart.c               \
        kern/proc.c                   \
        kern/sys.c                    \
        kern/frame.c                  \
        kern/util.c                   \
        lib/libstring/conv.c          \
        lib/libstring/scanf.c         \
        lib/libstring/string.c        \
        lib/libfdt/fdt.c

PROC0 = arm/proc0

USER.LD = arm/user.ld

