PROCS_IDLE_OUT = procs/idle/out.elf

PROCS_IDLE_SRC_C = \
	procs/idle/idle.c

PROCS_IDLE_LIBS = $(USER_LIBS)

PROCS_IDLE_OBJECTS = \
	$(PROCS_IDLE_SRC_C:%.c=%.o) \
	$(PROCS_IDLE_LIBS:%=%/lib.a)

$(PROCS_IDLE_OUT): $(PROCS_IDLE_OBJECTS) 
	@echo LD $@ $(OBJECTS)
	@$(LD) $(LDFLAGS_USER) \
		-o $@ \
		$(PROCS_IDLE_OBJECTS) \
		-lgcc

CLEAN += $(PROCS_IDLE_OBJECTS)
CLEAN += $(PROCS_IDLE_OUT)

