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

TARGET = am335x

KERNADDR=0x82000000
USERADDR=0x00010000

CROSS = arm-linux-gnueabihf-

CC = $(CROSS)gcc
LD = $(CROSS)ld
AR = $(CROSS)ar
OBJCOPY = $(CROSS)objcopy
OBJDUMP = $(CROSS)objdump

MKIMAGE = /home/mytch/src/u-boot/tools/mkimage

CFLAGS = -std=c89 \
	-Wall \
	-mcpu=cortex-a8 \
	-nostdinc -ffreestanding \
	-fno-stack-protector \
	-DKERNEL_ADDR=$(KERNADDR) \
	-DUSER_ADDR=$(USERADDR) \
	-Iinclude -I../include -Ikern/

LDFLAGS = -nostdlib -nodefaultlibs -static \
	-L/usr/lib/gcc/arm-linux-gnueabihf/7.2.0/ 


.PHONY: all
all: am335x.umg

CLEAN += proc0.list am335x.list am335x.umg 

.c.o .S.o:
	$(CC) $(CFLAGS) -c $< -o $@


.o.a:
	$(AR) rcs $@ $<


.elf.bin:
	$(OBJCOPY) -Obinary $< $@


.elf.list:
	$(OBJDUMP) -S $< > $@

bundled.bo: bundled.bin
	$(OBJCOPY) -B arm -O elf32-littlearm -I binary \
	    --rename-section .data=.bundled \
	    $< $@


CLEAN += bundled.bin bundled.list bundled.bo
bundled.bin bundled.h: proc0.bin proc1.bin
	sh bundle.sh bundled.bin bundled.list $^
	

P0_SRC := \
   ../lib/libc/sys.S        \
	proc0/main.c              \
  ../lib/libc/util.c
				

P0_OBJECTS_C := \
	$(P0_SRC:%.c=%.o)
P0_OBJECTS := \
	$(P0_OBJECTS_C:%.S=%.o)

CLEAN += $(P0_OBJECTS)
CLEAN += proc0.elf proc0.bin 

proc0.elf: proc0/linker.ld $(P0_OBJECTS)
	$(LD) $(LDFLAGS) \
		-T proc0/linker.ld -Ttext $(USERADDR) \
		-o $@ $(P0_OBJECTS) \
		-lgcc

P1_SRC := \
   ../lib/libc/sys.S        \
	proc1/main.c              \
  ../lib/libc/util.c
				

P1_OBJECTS_C := \
	$(P1_SRC:%.c=%.o)
P1_OBJECTS := \
	$(P1_OBJECTS_C:%.S=%.o)

CLEAN += $(P1_OBJECTS)
CLEAN += proc1.elf proc1.bin 

proc1.elf: proc1/linker.ld $(P1_OBJECTS)
	$(LD) $(LDFLAGS) \
		-T proc1/linker.ld -Ttext $(USERADDR) \
		-o $@ $(P1_OBJECTS) \
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


kern/main.o: bundled.list

KOBJECTS_C := \
	$(KSRC:%.c=%.o)
KOBJECTS := \
	$(KOBJECTS_C:%.S=%.o)

CLEAN += $(KOBJECTS)
CLEAN += *.elf *.bin *.list *.umg *.bo
	
am335x.elf: kern/linker.ld bundled.bo $(KOBJECTS)
	$(LD) $(LDFLAGS) \
		-T kern/linker.ld -Ttext $(KERNADDR)\
		-o $@ $(KOBJECTS) bundled.bo \
		-lgcc

am335x.umg: am335x.bin
	$(MKIMAGE) -T kernel -A arm -C none -a $(KERNADDR) -d $< $@

.PHONY: clean 
clean: 
	rm -f $(CLEAN)

