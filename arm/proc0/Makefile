PROC0_SRC_A = \
	../../lib/libc/sys.S

PROC0_SRC_C = \
	../proc0/main.c              \
	../proc0/pool.c              \
	../proc0/proc.c              \
	../proc0/mem.c               \
	../lib/libmem/mmu.c          \
	../../lib/libstring/conv.c   \
	../../lib/libstring/scanf.c  \
	../../lib/libstring/string.c \
	../../lib/libc/util.c        \
	devices.c                    \
	bundle.c
				
PROC0_OBJECTS = \
  $(PROC0_SRC_A:%.S=%.o) \
  $(PROC0_SRC_C:%.c=%.o)

CLEAN += $(PROC0_OBJECTS)
CLEAN += \
	../proc0/proc0.elf \
	../proc0/proc0.bin \
	../proc0/proc0.list

../proc0/proc0.elf: $(USER.LD) $(PROC0_OBJECTS)
	@echo LD $@
	$(LD) $(LDFLAGS) \
		-T $(USER.LD) \
		-Ttext $(USER_ADDR) \
		-o $@ $(PROC0_OBJECTS) \
		-lgcc

