all: out.elf out.bin out.list

include ${BASE}/rules.mk

CLEAN = out.elf out.bin out.list

OBJECTS = $(SRC:%.c=%.o)

CLEAN += $(OBJECTS)

LIBS += -lc -lstring -lgcc

out.elf: $(USER.LD) $(OBJECTS)
	@echo LD $@
	@$(LD) $(LDFLAGS) \
		-T $(USER.LD) \
		-Ttext ${USER_ADDR} \
		-o $@ \
		$(OBJECTS) \
		$(LIBS)

clean:
	@rm -f $(CLEAN)

