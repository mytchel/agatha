#
#  Copyright (c) 2017 Mytchel Hammond <mytch@lackname.org>
#  
#  Permission is hereby granted, free of charge, to any person
#  obtaining a copy of this software and associated documentation
#  files (the "Software"), to deal in the Software without
#  restriction, including without limitation the rights to use,
#  copy, modify, merge, publish, distribute, sublicense, and/or
#  sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following
#  conditions:
#  
#  The above copyright notice and this permission notice shall be
#  included in all copies or substantial kernions of the Software.
#  
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
#  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
#  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
#  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
#  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
#  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
#  OTHER DEALINGS IN THE SOFTWARE
#

.SUFFIXES:
.SUFFIXES: .c .S .h .o .a .elf .bin .list .umg .bo

loadaddr=0x82000000
TARGET = am335x

CROSS = arm-linux-gnueabihf-

CC = $(CROSS)gcc
LD = $(CROSS)ld
AR = $(CROSS)ar
OBJCOPY = $(CROSS)objcopy
OBJDUMP = $(CROSS)objdump

MKIMAGE = /home/mytch/src/u-boot/tools/mkimage

CFLAGS = -std=c89 \
	-Wall -Werror \
	-mcpu=cortex-a8 \
	-nostdinc -ffreestanding \
	-fpic -fpie \
	-fno-stack-protector \
	-Iinclude -I../include -Ikern/

LDFLAGS = -nostdlib -nodefaultlibs -static \
	-L/usr/lib/gcc/arm-linux-gnueabi/7.2.0/ 


.PHONY: all
all: proc0.list am335x.list am335x.umg

CLEAN += proc0.list am335x.list am335x.umg 

.c.o .S.o:
	$(CC) $(CFLAGS) -c $< -o $@


.o.a:
	$(AR) rcs $@ $<


.elf.bin:
	$(OBJCOPY) -Obinary $< $@


.elf.list:
	$(OBJDUMP) -S $< > $@


.elf.bo:
	$(OBJCOPY) --rename-section .text=.$*.text \
	           --rename-section .data=.$*.data \
	           --rename-section .bss=.$*.bss \
	           -x -X -S \
	            $< $@

P0_SRC := \
        ../lib/libc/sys.S         \
				proc0/main.c              \
        ../lib/libc/util.c
				

P0_OBJECTS_C := \
	$(P0_SRC:%.c=%.o)
P0_OBJECTS := \
	$(P0_OBJECTS_C:%.S=%.o)

CLEAN += $(P0_OBJECTS)
CLEAN += proc0.elf proc0.bo 

proc0.elf: proc0/linker.ld $(P0_OBJECTS)
	$(LD) $(LDFLAGS) \
		-T proc0/linker.ld \
		-o $@ $(P0_OBJECTS) \
		-lgcc
		

KSRC := \
        kern/vectors.S            \
        kern/asm.S                \
        kern/main.c               \
        kern/trap.c               \
        kern/timer.c              \
        kern/mem.c                \
        kern/proc.c               \
        kern/uart.c               \
        ../kern/proc.c            \
        ../kern/sys.c             \
        ../kern/frame.c           \
        ../lib/libstring/conv.c   \
        ../lib/libstring/scanf.c  \
        ../lib/libstring/string.c \
        ../lib/libc/util.c


KOBJECTS_C := \
	$(KSRC:%.c=%.o)
KOBJECTS := \
	$(KOBJECTS_C:%.S=%.o)

CLEAN += $(KOBJECTS)
CLEAN += $(TARGET).elf $(TARGET).bin $(TARGET).list proc0.bo

am335x.elf: kern/linker.ld proc0.bo $(KOBJECTS)
	$(LD) $(LDFLAGS) \
		-T kern/linker.ld -Ttext $(loadaddr) \
		-o $@ $(KOBJECTS) proc0.bo \
		-lgcc

am335x.umg: am335x.bin
	$(MKIMAGE) -T kernel -A arm -C none -a $(loadaddr) -d $< $@

.PHONY: clean 
clean: 
	rm -f $(CLEAN)

