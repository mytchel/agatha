CROSS ?= arm-none-eabi

CC = $(CROSS)-gcc
LD = $(CROSS)-ld
AR = $(CROSS)-ar
RANLIB = $(CROSS)-ranlib
OBJCOPY = $(CROSS)-objcopy
OBJDUMP = $(CROSS)-objdump

USER.LD = arm/user.ld
USER_ADDR = 0x00020000

CFLAGS += \
	-fno-stack-protector \
	-DUSER_ADDR=$(USER_ADDR) \
	-Iarm/include

LDFLAGS += \
  -Larm/lib \
  -L/usr/local/lib/gcc/$(CROSS)/6.3.1 

LDFLAGS_USER += \
	$(LDFLAGS) \
	-T $(USER.LD) \
	-Ttext $(USER_ADDR)

USER_LIBS += \
	arm/lib/c \
	lib/pool \
	lib/mem \
	lib/string

.bin.bo:
	@echo BO $@
	@$(OBJCOPY) -B arm -O elf32-littlearm -I binary \
		--rename-section .data=.bundle \
		$< $@

