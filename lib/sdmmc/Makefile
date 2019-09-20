LIB_SDMMC_LIB = lib/sdmmc/lib.a

LIB_SDMMC_SRC_A = 

LIB_SDMMC_SRC_C = \
	lib/sdmmc/sdmmc.c

LIB_SDMMC_OBJECTS = \
	$(LIB_SDMMC_SRC_A:%.S=%.o) \
	$(LIB_SDMMC_SRC_C:%.c=%.o) 

$(LIB_SDMMC_LIB): $(LIB_SDMMC_OBJECTS)
	@echo AR $@
	$(AR) rc $@ $(LIB_SDMMC_OBJECTS)
	$(RANLIB) $@

CLEAN += $(LIB_SDMMC_OBJECTS)
CLEAN += $(LIB_SDMMC_LIB)

