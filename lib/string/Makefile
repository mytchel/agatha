LIB_STRING_LIB = lib/string/lib.a

LIB_STRING_SRC_A = 

LIB_STRING_SRC_C = \
	lib/string/conv.c \
	lib/string/scanf.c \
	lib/string/string.c

LIB_STRING_OBJECTS = \
	$(LIB_STRING_SRC_A:%.S=%.o) \
	$(LIB_STRING_SRC_C:%.c=%.o) 

$(LIB_STRING_LIB): $(LIB_STRING_OBJECTS)
	@echo AR $@
	$(AR) rc $@ $(LIB_STRING_OBJECTS)
	$(RANLIB) $@

CLEAN += $(LIB_STRING_OBJECTS)
CLEAN += $(LIB_STRING_LIB)

