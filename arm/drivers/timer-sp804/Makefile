TIMER_SP804_OUT = arm/drivers/timer-sp804/out.elf

TIMER_SP804_SRC_C = \
	arm/drivers/timer-sp804/main.c

TIMER_SP804_LIBS = lib/log $(USER_LIBS)

TIMER_SP804_OBJECTS = \
	$(TIMER_SP804_SRC_C:%.c=%.o) \
	$(TIMER_SP804_LIBS:%=%/lib.a)

$(TIMER_SP804_OUT): $(TIMER_SP804_OBJECTS) 
	@echo LD $@ $(OBJECTS)
	@$(LD) $(LDFLAGS_USER) \
		-o $@ \
		$(TIMER_SP804_OBJECTS) \
		-lgcc

CLEAN += $(TIMER_SP804_OBJECTS)
CLEAN += $(TIMER_SP804_OUT)

