BASE = am335x/uart
TARGET = uart

.PHONY: $(BASE)
$(BASE): $(BASE)/$(TARGET).elf $(BASE)/$(TARGET).bin $(BASE)/$(TARGET).list

include Makefile.inc

CLEAN += $(BASE)/$(TARGET).elf $(BASE)/$(TARGET).bin $(BASE)/$(TARGET).list

SRC_A := \
  lib/libc/sys.S

SRC_C := \
  $(BASE)/main.c      \
  lib/libc/util.c
				
OBJECTS := $(SRC_A:%.S=%.o) $(SRC_C:%.c=%.o)

CLEAN += $(OBJECTS)

$(BASE)/$(TARGET).elf: $(BASE)/linker.ld $(OBJECTS)
	$(LD) $(LDFLAGS) \
		-T $(BASE)/linker.ld -Ttext $(USERADDR) \
		-o $@ $(OBJECTS) \
		-lgcc


.PHONY: clean 
clean: 
	rm -f $(CLEAN)

