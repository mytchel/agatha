
KERNADDR=0x82000000
USERADDR=0x00010000

.PHONY: all
all: am335x.umg am335x.list

BASE = ../
include Makefile.inc

CFLAGS += -Ikern

BUNDLED := proc0 uart

$(BUNDLED):
	$(MAKE) -C $@

bundled.bo: bundled.bin
	$(OBJCOPY) -B arm -O elf32-littlearm -I binary \
	    --rename-section .data=.bundled \
	    $< $@


CLEAN += bundled.bin bundled.list bundled.bo
bundled.bin bundled.list:
	for d in $(BUNDLED); \
	do \
		$(MAKE) -C $$d USERADDR=$(USERADDR); \
	done 
	sh bundle.sh bundled.bin bundled.list $(foreach b,$(BUNDLED),$b/$b.elf)
	

SRC_A := \
        kern/vectors.S            \
        kern/asm.S

SRC_C := \
        kern/main.c               \
        kern/trap.c               \
        kern/mem.c                \
        kern/proc.c               \
        kern/uart.c               \
        ../kern/proc.c            \
        ../kern/sys.c             \
        ../kern/frame.c           \
        ../lib/libstring/conv.c   \
        ../lib/libstring/scanf.c  \
        ../lib/libstring/string.c \
        ../lib/libfdt/fdt.c       \
        ../lib/libc/util.c


kern/main.o: bundled.list

OBJECTS := $(SRC_A:%.S=%.o) $(SRC_C:%.c=%.o) 

CLEAN += $(OBJECTS)
CLEAN += *.elf *.bin *.list *.umg *.bo
	
am335x.elf: kern/linker.ld bundled.bo $(OBJECTS)
	$(LD) $(LDFLAGS) \
		-T kern/linker.ld -Ttext $(KERNADDR)\
		-o $@ $(OBJECTS) bundled.bo \
		-lgcc


.PHONY: clean
clean:
	rm -f $(CLEAN)
	for d in $(BUNDLED); \
	do \
		$(MAKE) -C $$d clean; \
	done



