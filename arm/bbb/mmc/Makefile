OMAP_MMC_SRC_A := \
   lib/libc/sys.S

OMAP_MMC_SRC_C := \
  drivers/omap/mmc/mmchs.c     \
  drivers/omap/mmc/emmc.c      \
  drivers/omap/mmc/sd.c        \
  drivers/omap/mmc/misc.c      \
  lib/libc/util.c              \
  lib/libc/mem.c               \
  lib/libstring/conv.c         \
  lib/libstring/scanf.c        \
  lib/libstring/string.c
				

OMAP_MMC_OBJECTS := \
  $(OMAP_MMC_SRC_A:%.S=%.o) \
  $(OMAP_MMC_SRC_C:%.c=%.o)

CLEAN += $(OMAP_MMC_OBJECTS)

CLEAN += drivers/omap/mmc/mmc.elf \
         drivers/omap/mmc/mmc.bin \
         drivers/omap/mmc/mmc.list

drivers/omap/mmc/mmc.elf: $(USER.LD) $(OMAP_MMC_OBJECTS)
	@echo LD $@
	@$(LD) $(LDFLAGS) \
		-T $(USER.LD) -Ttext $(USER_ADDR) \
		-o $@ $(OMAP_MMC_OBJECTS) \
		-lgcc

