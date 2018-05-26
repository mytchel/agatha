ARCH = arm
CROSS = arm-none-eabi

CC = $(CROSS)-gcc
LD = $(CROSS)-ld
AR = $(CROSS)-ar
OBJCOPY = $(CROSS)-objcopy
OBJDUMP = $(CROSS)-objdump
MKUBOOT = $(BASE)/mkuboot

USER.LD = $(BASE)/arm/user.ld
USER_ADDR = 0x00010000

CFLAGS += \
	-fno-stack-protector \
	-DUSER_ADDR=$(USER_ADDR) \
	-I$(BASE)/arm/include
	
LDFLAGS += \
	-L/usr/local/lib/gcc/$(CROSS)/6.3.1

