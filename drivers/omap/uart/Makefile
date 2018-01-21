CLEAN += drivers/uart/uart.elf \
         drivers/uart/uart.bin \
         drivers/uart/uart.list

UART_SRC_A := \
   lib/libc/sys.S

UART_SRC_C := \
  drivers/uart/main.c              \
  lib/libc/util.c              \
  lib/libc/mem.c               \
  lib/libstring/conv.c         \
  lib/libstring/scanf.c        \
  lib/libstring/string.c
				

UART_OBJECTS := \
  $(UART_SRC_A:%.S=%.o) \
  $(UART_SRC_C:%.c=%.o)

CLEAN += $(UART_OBJECTS)

drivers/uart/uart.elf: $(USER.LD) $(UART_OBJECTS)
	@echo LD $@
	@$(LD) $(LDFLAGS) \
		-T $(USER.LD) -Ttext $(USER_ADDR) \
		-o $@ $(UART_OBJECTS) \
		-lgcc

