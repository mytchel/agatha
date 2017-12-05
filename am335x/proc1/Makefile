TARGET = proc1

.PHONY: all
all: $(TARGET).elf $(TARGET).bin $(TARGET).list

BASE = ../../
include ../Makefile.inc

CLEAN += $(TARGET).elf $(TARGET).bin $(TARGET).list

SRC_A := \
   $(BASE)/lib/libc/sys.S

SRC_C := \
  main.c              \
  $(BASE)/lib/libc/util.c
				

OBJECTS := $(SRC_A:%.S=%.o) $(SRC_C:%.c=%.o)

CLEAN += $(OBJECTS)

$(TARGET).elf: linker.ld $(OBJECTS)
	$(LD) $(LDFLAGS) \
		-T linker.ld -Ttext $(USERADDR) \
		-o $@ $(OBJECTS) \
		-lgcc


.PHONY: clean 
clean: 
	rm -f $(CLEAN)

