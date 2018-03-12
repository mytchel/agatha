all: kern.umg kern.list

BASE = ../..

include ../../Makefile

include ../Makefile

PROC0 = ../proc0

include $(PROC0)/Makefile

LOAD_ADDR = 0x82000000

CFLAGS += -I include
LDFLAGS +=

KERNEL_SRC_A += 

KERNEL_SRC_C += \
	kern/dev.c           \
	kern/am33xx-trap.c   \
	kern/omap3-uart.c    \
	kern/am335x-timer.c

KERNEL_OBJECTS := \
  $(KERNEL_SRC_A:%.S=%.o) \
  $(KERNEL_SRC_C:%.c=%.o)

CLEAN += $(KERNEL_OBJECTS)
CLEAN += kern.elf kern.bin kern.umg kern.list

# TODO: There must be a better way to do this

BUNDLE += $(BASE)/drivers/test 
BUNDLE_ELF += $(BASE)/drivers/test/test.elf
BUNDLE_BIN += $(BASE)/drivers/test/test.bin
include $(BASE)/drivers/test/Makefile

bundle.bin bundle.list: $(BUNDLE_ELF) $(BUNDLE_BIN)
	@sh $(BASE)/bundle.sh \
		bundle.bin \
		bundle.list \
		$(BUNDLE)

bundle.bo: bundle.bin

CLEAN += bundle.list
CLEAN += bundle.bin
CLEAN += bundle.bo

proc0.bin: $(PROC0)/proc0.elf
proc0.bo: proc0.bin
CLEAN += proc0.bo proc0.bin

kern.elf: ../kernel.ld proc0.bo $(KERNEL_OBJECTS)
	@echo LD $@
	@$(LD) $(LDFLAGS) \
		-T ../kernel.ld -Ttext $(LOAD_ADDR) \
		-o $@ $(KERNEL_OBJECTS) proc0.bo \
		-lgcc

