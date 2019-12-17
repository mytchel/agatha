PROCS_FS_OUT = procs/fs/out.elf

PROCS_FS_SRC_C = \
	procs/fs/main.c

PROCS_FS_LIBS = $(LIBC)

PROCS_FS_OBJECTS = \
	$(PROCS_FS_SRC_C:%.c=%.o) \
	$(PROCS_FS_LIBS:%=%/lib.a)

$(PROCS_FS_OUT): $(PROCS_FS_OBJECTS) 
	@echo LD $@ $(OBJECTS)
	@$(LD) $(LDFLAGS_USER) \
		-o $@ \
		$(PROCS_FS_OBJECTS) \
		-lgcc

CLEAN += $(PROCS_FS_OBJECTS)
CLEAN += $(PROCS_FS_OUT)

