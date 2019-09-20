LIB_POOL_LIB = lib/pool/lib.a

LIB_POOL_SRC_A = 

LIB_POOL_SRC_C = \
	lib/pool/pool.c

LIB_POOL_OBJECTS = \
	$(LIB_POOL_SRC_A:%.S=%.o) \
	$(LIB_POOL_SRC_C:%.c=%.o) 

$(LIB_POOL_LIB): $(LIB_POOL_OBJECTS)
	@echo AR $@
	@$(AR) rc $@ $(LIB_POOL_OBJECTS)
	@$(RANLIB) $@

CLEAN += $(LIB_POOL_OBJECTS)
CLEAN += $(LIB_POOL_LIB)

