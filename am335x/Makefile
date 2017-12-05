
KERNADDR=0x82000000

.PHONY: all
all: am335x.umg am335x.list

BASE = ../
include Makefile.inc

CFLAGS += -Ikern

BUNDLED := proc0 proc1 uart

$(BUNDLED):
	$(MAKE) -C $@

bundled.bo: bundled.bin
	$(OBJCOPY) -B arm -O elf32-littlearm -I binary \
	    --rename-section .data=.bundled \
	    $< $@


CLEAN += bundled.bin bundled.list bundled.bo
bundled.bin bundled.h:
	for d in $(BUNDLED); \
	do \
		$(MAKE) -C $$d; \
	done 
	sh bundle.sh bundled.bin bundled.list $(foreach b,$(BUNDLED),$b/$b.elf)
	

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


.PHONY: clean
clean:
	rm -f $(CLEAN)
	for d in $(BUNDLED); \
	do \
		$(MAKE) -C $$d clean; \
	done



