PROCS_TEST_OUT = procs/test/out.elf

PROCS_TEST_SRC_C = \
	procs/test/main.c \
	procs/test/timer.c \
	procs/test/mem.c \
	procs/test/fs.c \
	procs/test/net.c

PROCS_TEST_LIBS = \
	$(LIBC) \
	lib/pool \
	lib/log \
	lib/string \
	lib/mem \
	lib/fat

PROCS_TEST_OBJECTS = \
	$(PROCS_TEST_SRC_C:%.c=%.o) \
	$(PROCS_TEST_LIBS:%=%/lib.a)

$(PROCS_TEST_OUT): $(PROCS_TEST_OBJECTS) 
	@echo LD $@ 
	@$(LD) $(LDFLAGS_USER) \
		-o $@ \
		$(PROCS_TEST_OBJECTS) \
		-lgcc

CLEAN += $(PROCS_TEST_OBJECTS)
CLEAN += $(PROCS_TEST_OUT)

