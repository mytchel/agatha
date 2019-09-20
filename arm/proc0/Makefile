ARM_PROC0_OUT = arm/proc0/out.elf

ARM_PROC0_SRC_A = \
	arm/lib/c/sys.S \
	arm/proc0/kern.S

ARM_PROC0_SRC_C = \
	arm/proc0/main.c \
	arm/proc0/mem.c \
	arm/proc0/proc.c \
	arm/lib/c/addr.c \
	arm/lib/c/mmu.c \
	lib/mem/util.c \
	$(TARGET)/devices.c \
	bundle.c 
	
ARM_PROC0_LIBS = \
	lib/pool \
	lib/string 
			
ARM_PROC0_OBJECTS = \
	$(ARM_PROC0_SRC_A:%.S=%.o) \
	$(ARM_PROC0_SRC_C:%.c=%.o) \
	$(ARM_PROC0_LIBS:%=%/lib.a)

CLEAN += $(ARM_PROC0_OBJECTS)
CLEAN += $(ARM_PROC0_OUT)

$(ARM_PROC0_OUT): $(USER.LD) $(ARM_PROC0_OBJECTS)
	@echo LD $@ 
	@$(LD) $(LDFLAGS_USER) \
		-o $@ \
	 	$(ARM_PROC0_OBJECTS) \
	 	-lgcc


