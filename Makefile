.SUFFIXES: .c .S .h .o .a .elf .bin .list .umg .bo

LOAD_ADDR ?= 0x82000000
USER_ADDR ?= 0x00010000
ARCH ?= arm
CROSS ?= arm-linux-gnueabihf-

CC = $(CROSS)gcc
LD = $(CROSS)ld
AR = $(CROSS)ar
OBJCOPY = $(CROSS)objcopy
OBJDUMP = $(CROSS)objdump
MKIMAGE = mkimage

CFLAGS = \
         -std=c89 \
         -Wall \
         -nostdinc -ffreestanding \
         -fno-stack-protector \
         -DUSER_ADDR=$(USER_ADDR) \
         -Iinclude -I$(ARCH)/include 


LDFLAGS = -nostdlib -nodefaultlibs -static \
          -L/usr/lib/gcc/arm-linux-gnueabihf/7.2.0/ 


all: $(ARCH)/kern.umg $(ARCH)/kern.list
CLEAN += $(ARCH)/kern.umg $(ARCH)/kern.list

include $(ARCH)/Makefile
include $(PROC0)/Makefile

OBJECTS := $(SRC_A:%.S=%.o) $(SRC_C:%.c=%.o) 

CLEAN += $(OBJECTS) $(PROC0)/proc0.bin $(PROC0)/proc0.bo
CLEAN += $(ARCH)/kern.elf 

$(ARCH)/kern.elf: $(ARCH)/kernel.ld $(PROC0)/proc0.bo $(OBJECTS)
	$(LD) $(LDFLAGS) \
		-T $(ARCH)/kernel.ld -Ttext $(LOAD_ADDR) \
		-o $@ $(OBJECTS) $(PROC0)/proc0.bo \
		-lgcc


.PHONY: clean
clean:
	rm -f $(CLEAN)


.c.o .S.o:
	$(CC) $(CFLAGS) -c $< -o $@

.o.a:
	$(AR) rcs $@ $<

.elf.bin:
	$(OBJCOPY) -Obinary $< $@

.elf.list:
	$(OBJDUMP) -S $< > $@

.bin.umg: 
	$(MKIMAGE) -T kernel -A arm -C none -a $(LOAD_ADDR) -d $< $@

.bin.bo: 
	$(OBJCOPY) -B arm -O elf32-littlearm -I binary \
		--rename-section .data=.bundle \
		$< $@


