PROCS_UI_OUT = procs/ui/out.elf

PROCS_UI_SRC_C = \
	procs/ui/main.c

PROCS_UI_LIBS = lib/log $(USER_LIBS)

PROCS_UI_OBJECTS = \
	$(PROCS_UI_SRC_C:%.c=%.o) \
	$(PROCS_UI_LIBS:%=%/lib.a)

$(PROCS_UI_OUT): $(PROCS_UI_OBJECTS) 
	@echo LD $@ 
	$(LD) $(LDFLAGS_USER) \
		-o $@ \
		$(PROCS_UI_OBJECTS) \
		-lgcc

CLEAN += $(PROCS_UI_OBJECTS)
CLEAN += $(PROCS_UI_OUT)

