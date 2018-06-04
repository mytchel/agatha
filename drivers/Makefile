all: $(PROG).elf $(PROG).bin $(PROG).list

include ../../rules.mk

CLEAN = \
	$(PROG).elf \
	$(PROG).bin \
	$(PROG).list

OBJECTS = $(SRC:%.c=%.o)

CLEAN += $(OBJECTS)

$(PROG).elf: $(USER.LD) $(OBJECTS)
	@echo LD $@
	$(LD) $(LDFLAGS) \
		-T $(USER.LD) \
		-Ttext ${USER_ADDR} \
		-o $@ \
		$(OBJECTS) \
		-lc -lstring -lgcc 

clean:
	@echo Clean
	rm -f $(CLEAN)

