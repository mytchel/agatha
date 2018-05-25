all: kern.umg kern.list

BASE = ../..

include ../../Makefile

include ../Makefile

LOAD_ADDR = 0x62008000

CFLAGS += -I include -mcpu=cortex-a9
LDFLAGS +=

KERNEL_SRC_A += 

KERNEL_SRC_C += \
	kern/dev.c       \
	kern/trap.c      \
	kern/serial.c

KERNEL_OBJECTS := \
  $(KERNEL_SRC_A:%.S=%.o) \
  $(KERNEL_SRC_C:%.c=%.o)

CLEAN += $(KERNEL_OBJECTS)
CLEAN += kern.elf kern.bin kern.umg kern.list

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

DEVICE_TABLE = devices.c

include ../proc0/Makefile

proc0.bin: $(BASE)/arm/proc0/proc0.elf
proc0.bo: proc0.bin

CLEAN += proc0.bo proc0.bin

kern.elf: ../kernel.ld proc0.bo bundle.bo $(KERNEL_OBJECTS)
	@echo LD $@
	@$(LD) $(LDFLAGS) \
		-T ../kernel.ld -Ttext $(LOAD_ADDR) \
		-o $@ $(KERNEL_OBJECTS) proc0.bo bundle.bo \
		-lgcc

