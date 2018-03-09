VIRT_LOAD_ADDR ?= 0x80000

CFLAGS += -I arm/virt/include
LDFLAGS +=

VIRT_KERNEL_SRC_A = $(ARM_KERNEL_SRC_A) 

VIRT_KERNEL_SRC_C = $(ARM_KERNEL_SRC_C) \
	arm/virt/dev.c     \
	arm/virt/serial.c  \
	arm/virt/timer.c   \
	arm/virt/trap.c

VIRT_KERNEL_OBJECTS := \
  $(VIRT_KERNEL_SRC_A:%.S=%.o) \
  $(VIRT_KERNEL_SRC_C:%.c=%.o)

CLEAN += $(VIRT_KERNEL_OBJECTS)
CLEAN += arm/virt/kern.elf arm/virt/kern.bin arm/virt/kern.umg

arm/virt/kern.elf: arm/kernel.ld $(PROC0)/proc0.bo $(VIRT_KERNEL_OBJECTS)
	@echo LD $@
	@$(LD) $(LDFLAGS) \
		-T arm/kernel.ld -Ttext $(VIRT_LOAD_ADDR) \
		-o $@ $(VIRT_KERNEL_OBJECTS) $(PROC0)/proc0.bo \
		-lgcc

