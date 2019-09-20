MMC_PL18X_OUT = arm/drivers/mmc-pl18x/out.elf

MMC_PL18X_SRC_C = \
	arm/drivers/mmc-pl18x/pl18x.c

MMC_PL18X_LIBS = \
	lib/sdmmc \
	lib/block \
	lib/log \
	$(USER_LIBS)

MMC_PL18X_OBJECTS = \
	$(MMC_PL18X_SRC_C:%.c=%.o) \
	$(MMC_PL18X_LIBS:%=%/lib.a)

$(MMC_PL18X_OUT): $(MMC_PL18X_OBJECTS) 
	@echo LD $@ $(OBJECTS)
	@$(LD) $(LDFLAGS_USER) \
		-o $@ \
		$(MMC_PL18X_OBJECTS) \
		-lgcc

CLEAN += $(MMC_PL18X_OBJECTS)
CLEAN += $(MMC_PL18X_OUT)

