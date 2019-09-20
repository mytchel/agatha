LIB_LOG_LIB = lib/log/lib.a

LIB_LOG_SRC_A = 

LIB_LOG_SRC_C = \
	lib/log/log.c

LIB_LOG_OBJECTS = \
	$(LIB_LOG_SRC_A:%.S=%.o) \
	$(LIB_LOG_SRC_C:%.c=%.o) 

$(LIB_LOG_LIB): $(LIB_LOG_OBJECTS)
	@echo AR $@
	@$(AR) rc $@ $(LIB_LOG_OBJECTS)
	@$(RANLIB) $@

CLEAN += $(LIB_LOG_OBJECTS)
CLEAN += $(LIB_LOG_LIB)

