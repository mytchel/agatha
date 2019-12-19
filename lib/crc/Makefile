LIB_CRC_LIB = lib/crc/lib.a

LIB_CRC_SRC_A = 

LIB_CRC_SRC_C = \
	lib/crc/crc.c

LIB_CRC_OBJECTS = \
	$(LIB_CRC_SRC_A:%.S=%.o) \
	$(LIB_CRC_SRC_C:%.c=%.o) 

$(LIB_CRC_LIB): $(LIB_CRC_OBJECTS)
	@echo AR $@
	@$(AR) rc $@ $(LIB_CRC_OBJECTS)
	@$(RANLIB) $@

CLEAN += $(LIB_CRC_OBJECTS)
CLEAN += $(LIB_CRC_LIB)

