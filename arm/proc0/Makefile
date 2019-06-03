PROC0_SRC_A = \
	../lib/c/sys.S               \
	../proc0/kern.S

PROC0_SRC_C = \
	../proc0/main.c              \
	../proc0/mem.c               \
	../proc0/proc.c              \
	../lib/c/addr.c              \
	../lib/c/mmu.c               \
	../../lib/mem/util.c         \
	devices.c                    \
	bundle.c
				
PROC0_OBJECTS = \
								$(PROC0_SRC_A:%.S=%.o) \
								$(PROC0_SRC_C:%.c=%.o)

PROC0_LIBS = -lpool -lstring -lgcc

CLEAN += $(PROC0_OBJECTS)
CLEAN += \
	../proc0/proc0.elf \
	../proc0/proc0.bin \
	../proc0/proc0.list

../proc0/proc0.elf: $(USER.LD) $(PROC0_OBJECTS)
	@echo LD $@ $(PROC0_OBJECTS) $(PROC0_LIBS)
	@$(LD) $(LDFLAGS) \
		-T $(USER.LD) \
		-Ttext $(USER_ADDR) \
		-o $@ \
	 	$(PROC0_OBJECTS) \
	 	$(PROC0_LIBS)


