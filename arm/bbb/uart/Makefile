OMAP_UART_SRC_A := \
   lib/libc/sys.S

OMAP_UART_SRC_C := \
  drivers/omap/uart/main.c     \
  lib/libc/util.c              \
  lib/libc/mem.c               \
  lib/libstring/conv.c         \
  lib/libstring/scanf.c        \
  lib/libstring/string.c
				

OMAP_UART_OBJECTS := \
  $(OMAP_UART_SRC_A:%.S=%.o) \
  $(OMAP_UART_SRC_C:%.c=%.o)

CLEAN += $(OMAP_UART_OBJECTS)

CLEAN += drivers/omap/uart/uart.elf \
         drivers/omap/uart/uart.bin \
         drivers/omap/uart/uart.list

drivers/omap/uart/uart.elf: $(USER.LD) $(OMAP_UART_OBJECTS)
	@echo LD $@
	@$(LD) $(LDFLAGS) \
		-T $(USER.LD) -Ttext $(USER_ADDR) \
		-o $@ $(OMAP_UART_OBJECTS) \
		-lgcc

