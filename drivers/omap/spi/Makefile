SPI_SRC_A := \
   lib/libc/sys.S

SPI_SRC_C := \
  drivers/omap/spi/main.c      \
  lib/libc/util.c              \
  lib/libc/mem.c               \
  lib/libstring/conv.c         \
  lib/libstring/scanf.c        \
  lib/libstring/string.c
				

SPI_OBJECTS := \
  $(SPI_SRC_A:%.S=%.o) \
  $(SPI_SRC_C:%.c=%.o)

CLEAN += $(SPI_OBJECTS) 

CLEAN += drivers/omap/spi/spi.elf \
         drivers/omap/spi/spi.bin \
         drivers/omap/spi/spi.list

drivers/omap/spi/spi.elf: $(USER.LD) $(SPI_OBJECTS)
	@echo LD $@
	@$(LD) $(LDFLAGS) \
		-T $(USER.LD) -Ttext $(USER_ADDR) \
		-o $@ $(SPI_OBJECTS) \
		-lgcc

