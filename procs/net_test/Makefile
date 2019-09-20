PROCS_NET_TEST_OUT = procs/net_test/out.elf

PROCS_NET_TEST_SRC_C = \
	procs/net_test/main.c

PROCS_NET_TEST_LIBS = lib/log $(USER_LIBS)

PROCS_NET_TEST_OBJECTS = \
	$(PROCS_NET_TEST_SRC_C:%.c=%.o) \
	$(PROCS_NET_TEST_LIBS:%=%/lib.a)

$(PROCS_NET_TEST_OUT): $(PROCS_NET_TEST_OBJECTS) 
	@echo LD $@ 
	$(LD) $(LDFLAGS_USER) \
		-o $@ \
		$(PROCS_NET_TEST_OBJECTS) \
		-lgcc

CLEAN += $(PROCS_NET_TEST_OBJECTS)
CLEAN += $(PROCS_NET_TEST_OUT)

