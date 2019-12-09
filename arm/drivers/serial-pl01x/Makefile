SERIAL_PL01X_OUT = arm/drivers/serial-pl01x/out.elf

SERIAL_PL01X_SRC_C = \
	arm/drivers/serial-pl01x/main.c

SERIAL_PL01X_LIBS = $(LIBC) lib/pool lib/string

SERIAL_PL01X_OBJECTS = \
	$(SERIAL_PL01X_SRC_C:%.c=%.o) \
	$(SERIAL_PL01X_LIBS:%=%/lib.a)

$(SERIAL_PL01X_OUT): $(SERIAL_PL01X_OBJECTS) 
	@echo LD $@ $(OBJECTS)
	@$(LD) $(LDFLAGS_USER) \
		-o $@ \
		$(SERIAL_PL01X_OBJECTS) \
		-lgcc

CLEAN += $(SERIAL_PL01X_OBJECTS)
CLEAN += $(SERIAL_PL01X_OUT)

