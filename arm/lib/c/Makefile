ARM_LIB_C_LIB = arm/lib/c/lib.a

ARM_LIB_C_SRC_A = \
	arm/lib/c/sys.S

ARM_LIB_C_SRC_C = \
	arm/lib/c/mmu.c \
	arm/lib/c/proc0.c \
	arm/lib/c/addr.c

ARM_LIB_C_OBJECTS = \
	$(ARM_LIB_C_SRC_A:%.S=%.o) \
	$(ARM_LIB_C_SRC_C:%.c=%.o) 

$(ARM_LIB_C_LIB): $(ARM_LIB_C_OBJECTS)
	@echo AR $@
	@$(AR) rc $@ $(ARM_LIB_C_OBJECTS)
	@$(RANLIB) $@

CLEAN += $(ARM_LIB_C_OBJECTS)
CLEAN += $(ARM_LIB_C_LIB)

