ARM_ROOT_OUT = arm/root/out.elf

ARM_ROOT_SRC_A = \
	arm/lib/c/syscall.S \
	arm/root/kern.S

ARM_ROOT_SRC_C = \
	arm/root/main.c \
	arm/root/mem.c \
	arm/root/proc.c \
	arm/lib/c/mmu.c \
	arm/lib/c/sys.c \
	lib/mem/util.c \
	$(TARGET)/devices.c \
	bundle.c 
	
#arm/lib/c/addr.c \
	
ARM_ROOT_LIBS = \
	lib/pool \
	lib/string 
			
ARM_ROOT_OBJECTS = \
	$(ARM_ROOT_SRC_A:%.S=%.o) \
	$(ARM_ROOT_SRC_C:%.c=%.o) \
	$(ARM_ROOT_LIBS:%=%/lib.a)

CLEAN += $(ARM_ROOT_OBJECTS)
CLEAN += $(ARM_ROOT_OUT)
CLEAN += $(ARM_ROOT_OUT:%.elf=%.bin)
CLEAN += $(ARM_ROOT_OUT:%.elf=%.list)

$(ARM_ROOT_OUT): $(USER.LD) $(ARM_ROOT_OBJECTS)
	@echo LD $@ 
	@$(LD) $(LDFLAGS_USER) \
		-o $@ \
	 	$(ARM_ROOT_OBJECTS) \
	 	-lgcc


