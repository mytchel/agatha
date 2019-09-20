MMC_OMAP3_OUT = arm/drivers/mmc-omap3/out.elf

MMC_OMAP3_SRC_C = \
	arm/drivers/mmc-omap3/mmc.c

MMC_OMAP3_LIBS = \
	lib/sdmmc \
	lib/block \
	lib/log \
	$(USER_LIBS)

MMC_OMAP3_OBJECTS = \
	$(MMC_OMAP3_SRC_C:%.c=%.o) \
	$(MMC_OMAP3_LIBS:%=%/lib.a)

$(MMC_OMAP3_OUT): $(MMC_OMAP3_OBJECTS) 
	@echo LD $@ $(OBJECTS)
	$(LD) $(LDFLAGS_USER) \
		-o $@ \
		$(MMC_OMAP3_OBJECTS) \
		-lgcc

CLEAN += $(MMC_OMAP3_OBJECTS)
CLEAN += $(MMC_OMAP3_OUT)

