CROSS ?= arm-none-eabi

CC = $(CROSS)-gcc
LD = $(CROSS)-ld
AR = $(CROSS)-ar
RANLIB = $(CROSS)-ranlib
OBJCOPY = $(CROSS)-objcopy
OBJDUMP = $(CROSS)-objdump

USER.LD = $(BASE)/arm/user.ld
USER_ADDR = 0x00020000

CFLAGS += \
	-fno-stack-protector \
	-DUSER_ADDR=$(USER_ADDR) \
	-I$(BASE)/arm/include

LDFLAGS += \
  -L${BASE}/arm/lib \
  -L/usr/local/lib/gcc/$(CROSS)/6.3.1 

include ${BASE}/${ARCH}/${BOARD}/rules.mk

.bin.bo:
	@echo BO $@
	@$(OBJCOPY) -B arm -O elf32-littlearm -I binary \
		--rename-section .data=.bundle \
		$< $@

