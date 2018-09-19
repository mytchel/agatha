all: out.elf out.bin out.list

include ../../../../rules.mk

CLEAN = out.elf out.bin out.list

OBJECTS = $(SRC:%.c=%.o)

CLEAN += $(OBJECTS)

out.elf: $(USER.LD) $(OBJECTS)
	@echo LD $@
	@$(LD) $(LDFLAGS) \
		-T $(USER.LD) \
		-Ttext ${USER_ADDR} \
		-o $@ \
		$(OBJECTS) \
		-lc -lstring -lgcc 

clean:
	@rm -f $(CLEAN)

