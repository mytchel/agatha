SERIAL_OMAP3_OUT = arm/drivers/serial-omap3/out.elf

SERIAL_OMAP3_SRC_C = \
	arm/drivers/serial-omap3/main.c

SERIAL_OMAP3_LIBS = $(USER_LIBS)

SERIAL_OMAP3_OBJECTS = \
	$(SERIAL_OMAP3_SRC_C:%.c=%.o) \
	$(SERIAL_OMAP3_LIBS:%=%/lib.a)

$(SERIAL_OMAP3_OUT): $(SERIAL_OMAP3_OBJECTS) 
	@echo LD $@ $(OBJECTS)
	$(LD) $(LDFLAGS_USER) \
		-o $@ \
		$(SERIAL_OMAP3_OBJECTS) \
		-lgcc

CLEAN += $(SERIAL_OMAP3_OBJECTS)
CLEAN += $(SERIAL_OMAP3_OUT)

