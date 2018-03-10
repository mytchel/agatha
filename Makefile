.SUFFIXES: .c .S .h .o .a .elf .bin .list .umg .bo

all: arm/bbb/kern.umg

ARCH ?= arm
CROSS ?= arm-none-eabi

CC = $(CROSS)-gcc
LD = $(CROSS)-ld
AR = $(CROSS)-ar
OBJCOPY = $(CROSS)-objcopy
OBJDUMP = $(CROSS)-objdump
MKUBOOT = mkuboot

CFLAGS = \
				 -std=c89 \
				 -Wall \
				 -nostdinc -ffreestanding \
				 -DUSER_ADDR=$(USER_ADDR) \
				 -DLOAD_ADDR=$(LOAD_ADDR) \
				 -Iinclude


LDFLAGS = -nostdlib -nodefaultlibs -static \
					-L/usr/local/lib/gcc/$(CROSS)/6.3.1

include $(ARCH)/Makefile

.PHONY: clean
clean:
	@echo CLEAN
	rm -f $(CLEAN)


.c.o .S.o:
	@echo CC $@
	@$(CC) $(CFLAGS) -c $< -o $@

.o.a:
	@echo AR $@
	@$(AR) rcs $@ $<

.elf.bin:
	@echo OBJCOPY $@
	@$(OBJCOPY) -Obinary $< $@

.elf.list:
	@echo OBJDUMP $@
	@$(OBJDUMP) -S $< > $@

.bin.umg: 
	@make -C tools/mkuboot
	@echo MKUBOOT $@
	@tools/mkuboot/mkuboot \
		-a arm -o linux -t kernel \
		-e $(LOAD_ADDR) -l $(LOAD_ADDR) \
		$< $@

.bin.bo: 
	@echo OBJCOPY $@
	@$(OBJCOPY) -B arm -O elf32-littlearm -I binary \
		--rename-section .data=.bundle \
		$< $@

