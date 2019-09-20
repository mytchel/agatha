LIB_BLOCK_LIB = lib/block/lib.a

LIB_BLOCK_SRC_A = 

LIB_BLOCK_SRC_C = \
	lib/block/block.c

LIB_BLOCK_OBJECTS = \
	$(LIB_BLOCK_SRC_A:%.S=%.o) \
	$(LIB_BLOCK_SRC_C:%.c=%.o) 

$(LIB_BLOCK_LIB): $(LIB_BLOCK_OBJECTS)
	@echo AR $@
	$(AR) rc $@ $(LIB_BLOCK_OBJECTS)
	$(RANLIB) $@

CLEAN += $(LIB_BLOCK_OBJECTS)
CLEAN += $(LIB_BLOCK_LIB)

