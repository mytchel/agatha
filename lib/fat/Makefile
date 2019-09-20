LIB_FAT_LIB = lib/fat/lib.a

LIB_FAT_SRC_A = 

LIB_FAT_SRC_C = \
	lib/fat/fat.c

LIB_FAT_OBJECTS = \
	$(LIB_FAT_SRC_A:%.S=%.o) \
	$(LIB_FAT_SRC_C:%.c=%.o) 

$(LIB_FAT_LIB): $(LIB_FAT_OBJECTS)
	@echo AR $@
	@$(AR) rc $@ $(LIB_FAT_OBJECTS)
	@$(RANLIB) $@

CLEAN += $(LIB_FAT_OBJECTS)
CLEAN += $(LIB_FAT_LIB)

