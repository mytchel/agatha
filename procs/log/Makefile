PROCS_LOG_OUT = procs/log/out.elf

PROCS_LOG_SRC_C = \
	procs/log/log.c

PROCS_LOG_LIBS = $(LIBC) lib/pool lib/string

PROCS_LOG_OBJECTS = \
	$(PROCS_LOG_SRC_C:%.c=%.o) \
	$(PROCS_LOG_LIBS:%=%/lib.a)

$(PROCS_LOG_OUT): $(PROCS_LOG_OBJECTS) 
	@echo LD $@ 
	@$(LD) $(LDFLAGS_USER) \
		-o $@ \
		$(PROCS_LOG_OBJECTS) \
		-lgcc

CLEAN += $(PROCS_LOG_OBJECTS)
CLEAN += $(PROCS_LOG_OUT)

