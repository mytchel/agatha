ARM_PROC1_OUT = arm/proc1/out.elf

ARM_PROC1_SRC_A = \
	arm/lib/c/sys.S \
	arm/proc1/kern.S

ARM_PROC1_SRC_C = \
	arm/proc1/main.c \
	arm/proc1/mem.c \
	arm/proc1/proc.c \
	arm/lib/c/addr.c \
	arm/lib/c/mmu.c \
	lib/mem/util.c \
	$(TARGET)/devices.c \
	bundle.c 
	
ARM_PROC1_LIBS = \
	lib/pool \
	lib/string 
			
ARM_PROC1_OBJECTS = \
	$(ARM_PROC1_SRC_A:%.S=%.o) \
	$(ARM_PROC1_SRC_C:%.c=%.o) \
	$(ARM_PROC1_LIBS:%=%/lib.a)

CLEAN += $(ARM_PROC1_OBJECTS)
CLEAN += $(ARM_PROC1_OUT)

$(ARM_PROC1_OUT): $(USER.LD) $(ARM_PROC1_OBJECTS)
	@echo LD $@ 
	@$(LD) $(LDFLAGS_USER) \
		-o $@ \
	 	$(ARM_PROC1_OBJECTS) \
	 	-lgcc


