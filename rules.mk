.SUFFIXES: .c .S .h .o .a .elf .bin .list .umg .bo

CFLAGS = \
	-std=c89 \
	-Wall \
	-nostdinc -ffreestanding \
	-I$(BASE)/include

LDFLAGS = -nostdlib -nodefaultlibs -static

.c.o .S.o:
	@echo CC $@
	$(CC) $(CFLAGS) -c $< -o $@

.o.a:
	@echo AR $@
	@$(AR) rcs $@ $<

.elf.bin:
	@echo OBJCOPY $@
	$(OBJCOPY) -Obinary $< $@

.elf.list:
	@echo OBJDUMP $@
	$(OBJDUMP) -S $< > $@

.bin.umg: 
	@make -C $(BASE)/tools/mkuboot
	@echo MKUBOOT $@
	@$(BASE)/tools/mkuboot/mkuboot \
		-a arm -o linux -t kernel \
		-e $(LOAD_ADDR) -l $(LOAD_ADDR) \
		$< $@

.bin.bo: 
	@echo OBJCOPY $@
	$(OBJCOPY) -B arm -O elf32-littlearm -I binary \
		--rename-section .data=.bundle \
		$< $@

BASE = /home/mytch/dev/os/arm/virt/../..
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
	-I$(BASE)/arm/include
	
LDFLAGS += \
	-L/usr/local/lib/gcc/$(CROSS)/6.3.1

CFLAGS += -I include -mcpu=cortex-a9
LDFLAGS +=

