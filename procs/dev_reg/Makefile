PROCS_DEV_REG_OUT = procs/dev_reg/out.elf

PROCS_DEV_REG_SRC_C = \
	procs/dev_reg/main.c

PROCS_DEV_REG_LIBS = $(USER_LIBS)

PROCS_DEV_REG_OBJECTS = \
	$(PROCS_DEV_REG_SRC_C:%.c=%.o) \
	$(PROCS_DEV_REG_LIBS:%=%/lib.a)

$(PROCS_DEV_REG_OUT): $(PROCS_DEV_REG_OBJECTS) 
	@echo LD $@ $(OBJECTS)
	@$(LD) $(LDFLAGS_USER) \
		-o $@ \
		$(PROCS_DEV_REG_OBJECTS) \
		-lgcc

CLEAN += $(PROCS_DEV_REG_OBJECTS)
CLEAN += $(PROCS_DEV_REG_OUT)

