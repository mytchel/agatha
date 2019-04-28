all: out.elf out.bin 

include ${BASE}/rules.mk

CLEAN = out.elf out.bin out.list

OBJECTS = $(SRC:%.c=%.o)

CLEAN += $(OBJECTS)

LIBS += -lc -lpool -lstring -lgcc

out.elf: $(USER.LD) $(OBJECTS)
	@echo LD $@ $(OBJECTS) $(LIBS)
	@$(LD) $(LDFLAGS) \
		-T $(USER.LD) \
		-Ttext ${USER_ADDR} \
		-o $@ \
		$(OBJECTS) \
		$(LIBS)

clean:
	@rm -f $(CLEAN)

