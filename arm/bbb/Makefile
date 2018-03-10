LOAD_ADDR ?= 0x82000000

CFLAGS += -I arm/bbb/include
LDFLAGS +=

BBB_KERNEL_SRC_A = $(ARM_KERNEL_SRC_A) 

BBB_KERNEL_SRC_C = $(ARM_KERNEL_SRC_C) \
	arm/bbb/dev.c           \
	arm/bbb/am33xx-trap.c   \
	arm/bbb/omap3-uart.c    \
	arm/bbb/am335x-timer.c

BBB_KERNEL_OBJECTS := \
  $(BBB_KERNEL_SRC_A:%.S=%.o) \
  $(BBB_KERNEL_SRC_C:%.c=%.o)

CLEAN += $(BBB_KERNEL_OBJECTS)
CLEAN += arm/bbb/kern.elf arm/bbb/kern.bin arm/bbb/kern.umg

arm/bbb/kern.elf: arm/kernel.ld $(PROC0)/proc0.bo $(BBB_KERNEL_OBJECTS)
	@echo LD $@
	@$(LD) $(LDFLAGS) \
		-T arm/kernel.ld -Ttext $(LOAD_ADDR) \
		-o $@ $(BBB_KERNEL_OBJECTS) $(PROC0)/proc0.bo \
		-lgcc

