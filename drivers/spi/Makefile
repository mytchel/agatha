CLEAN += drivers/spi/spi.elf \
         drivers/spi/spi.bin \
         drivers/spi/spi.list

SPI_SRC_A := \
   lib/libc/sys.S

SPI_SRC_C := \
  drivers/spi/main.c              \
  lib/libc/util.c              \
  lib/libc/mem.c               \
  lib/libstring/conv.c         \
  lib/libstring/scanf.c        \
  lib/libstring/string.c
				

SPI_OBJECTS := \
  $(SPI_SRC_A:%.S=%.o) \
  $(SPI_SRC_C:%.c=%.o)

CLEAN += $(SPI_OBJECTS)

drivers/spi/spi.elf: $(USER.LD) $(SPI_OBJECTS)
	@echo LD $@
	@$(LD) $(LDFLAGS) \
		-T $(USER.LD) -Ttext $(USER_ADDR) \
		-o $@ $(SPI_OBJECTS) \
		-lgcc

