PROC0_SRC = \
	../proc0/main.c              \
	../proc0/pool.c              \
	../proc0/proc.c              \
	../proc0/mem.c               \
	devices.c                    \
	bundle.c
				
PROC0_OBJECTS = $(PROC0_SRC:%.c=%.o)

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
		-o $@ \
	 	$(PROC0_OBJECTS) \
		-lc -lstring -lgcc


