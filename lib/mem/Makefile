LIB_MEM_LIB = lib/mem/lib.a

LIB_MEM_SRC_A = 

LIB_MEM_SRC_C = \
	lib/mem/alloc.c \
	lib/mem/util.c

LIB_MEM_OBJECTS = \
	$(LIB_MEM_SRC_A:%.S=%.o) \
	$(LIB_MEM_SRC_C:%.c=%.o) 

$(LIB_MEM_LIB): $(LIB_MEM_OBJECTS)
	@echo AR $@
	@$(AR) rc $@ $(LIB_MEM_OBJECTS)
	@$(RANLIB) $@

CLEAN += $(LIB_MEM_OBJECTS)
CLEAN += $(LIB_MEM_LIB)

