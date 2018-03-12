all: kern.elf kern.list

BASE = ../..

.include "../Makefile"

LOAD_ADDR = 0x20000000

CFLAGS += -I include
LDFLAGS +=

KERNEL_SRC_A += 

KERNEL_SRC_C += \
	kern/dev.c           \
	kern/trap.c   \
	kern/serial.c    \
	kern/timer.c

KERNEL_OBJECTS := \
  $(KERNEL_SRC_A:%.S=%.o) \
  $(KERNEL_SRC_C:%.c=%.o)

CLEAN += $(KERNEL_OBJECTS)
CLEAN += kern.elf kern.bin kern.umg kern.list

proc0.bin: $(PROC0)/proc0.elf
proc0.bo: proc0.bin
CLEAN += proc0.bo proc0.bin

kern.elf: ../kernel.ld proc0.bo $(KERNEL_OBJECTS)
	@echo LD $@
	@$(LD) $(LDFLAGS) \
		-T ../kernel.ld -Ttext $(LOAD_ADDR) \
		-o $@ $(KERNEL_OBJECTS) proc0.bo \
		-lgcc

