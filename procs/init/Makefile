PROCS_INIT_OUT = procs/init/out.elf

PROCS_INIT_SRC_C = \
	procs/init/main.c \
	procs/init/timer.c

PROCS_INIT_LIBS = \
	$(LIBC) \
	lib/pool \
	lib/log \
	lib/string

PROCS_INIT_OBJECTS = \
	$(PROCS_INIT_SRC_C:%.c=%.o) \
	$(PROCS_INIT_LIBS:%=%/lib.a)

$(PROCS_INIT_OUT): $(PROCS_INIT_OBJECTS) 
	@echo LD $@ 
	@$(LD) $(LDFLAGS_USER) \
		-o $@ \
		$(PROCS_INIT_OBJECTS) \
		-lgcc

CLEAN += $(PROCS_INIT_OBJECTS)
CLEAN += $(PROCS_INIT_OUT)

