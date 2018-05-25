CLEAN += \
	$(BASE)/arm/proc0/proc0.elf \
	$(BASE)/arm/proc0/proc0.bin \
	$(BASE)/arm/proc0/proc0.list

PROC0_SRC_A = \
	$(BASE)/lib/libc/sys.S

PROC0_SRC_C += \
	$(BASE)/arm/proc0/main.c              \
	$(BASE)/arm/proc0/proc.c              \
	$(BASE)/arm/proc0/mem.c               \
	$(BASE)/arm/proc0/mmu.c               \
	$(BASE)/lib/libfdt/fdt.c              \
	$(BASE)/lib/libstring/conv.c          \
	$(BASE)/lib/libstring/scanf.c         \
	$(BASE)/lib/libstring/string.c        \
	$(BASE)/lib/libc/util.c               \
	$(DEVICE_TABLE)
				
PROC0_OBJECTS = \
  $(PROC0_SRC_A:%.S=%.o) \
  $(PROC0_SRC_C:%.c=%.o)

CLEAN += $(PROC0_OBJECTS)

$(BASE)/arm/proc0/proc0.elf: \
		$(USER.LD) \
		$(BASE)/arm/proc0/linker.ld \
		$(PROC0_OBJECTS)
	@echo LD $@
	@$(LD) $(LDFLAGS) \
		-T $(BASE)/arm/proc0/linker.ld \
		-Ttext $(USER_ADDR) \
		-o $@ $(PROC0_OBJECTS) \
		-lgcc

