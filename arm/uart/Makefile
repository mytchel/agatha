CLEAN += arm/uart/uart.elf \
         arm/uart/uart.bin \
         arm/uart/uart.list

UART_SRC_A := \
   lib/libc/sys.S

UART_SRC_C := \
  arm/uart/main.c              \
  lib/libc/util.c
				

UART_OBJECTS := \
  $(UART_SRC_A:%.S=%.o) \
  $(UART_SRC_C:%.c=%.o)

CLEAN += $(UART_OBJECTS)

arm/uart/uart.elf: arm/user.ld $(UART_OBJECTS)
	@echo LD $@
	@$(LD) $(LDFLAGS) \
		-T arm/user.ld -Ttext $(USER_ADDR) \
		-o $@ $(UART_OBJECTS) \
		-lgcc

