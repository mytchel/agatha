PROCS_LFS_OUT = procs/lfs/out.elf

PROCS_LFS_SRC_C = \
	procs/lfs/main.c \
	procs/lfs/lfs.c \
	procs/lfs/help.c

PROCS_LFS_LIBS = \
	lib/log \
	lib/string \
	lib/mem \
	lib/crc \
	$(LIBC) \
	lib/pool 

PROCS_LFS_OBJECTS = \
	$(PROCS_LFS_SRC_C:%.c=%.o) \
	$(PROCS_LFS_LIBS:%=%/lib.a)

$(PROCS_LFS_OUT): $(PROCS_LFS_OBJECTS) 
	@echo LD $@ $(OBJECTS)
	@$(LD) $(LDFLAGS_USER) \
		-o $@ \
		$(PROCS_LFS_OBJECTS) \
		-lgcc

CLEAN += $(PROCS_LFS_OBJECTS)
CLEAN += $(PROCS_LFS_OUT)

