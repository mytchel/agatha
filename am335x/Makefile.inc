.SUFFIXES: .c .S .h .o .a .elf .bin .list .umg .bo

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
	-I$(BASE)/include -I$(BASE)/am335x/include 


LDFLAGS = -nostdlib -nodefaultlibs -static \
	-L/usr/lib/gcc/arm-linux-gnueabihf/7.2.0/ 


.c.o .S.o:
	$(CC) $(CFLAGS) -c $< -o $@


.o.a:
	$(AR) rcs $@ $<


.elf.bin:
	$(OBJCOPY) -Obinary $< $@


.elf.list:
	$(OBJDUMP) -S $< > $@


.bin.umg: 
	$(MKIMAGE) -T kernel -A arm -C none -a $(KERNADDR) -d $< $@


