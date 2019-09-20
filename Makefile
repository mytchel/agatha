include rules.mk

ARCH = arm
TARGET = arm/beagleboneblack

CFLAGS += -DDEBUG_LEVEL=3
#CFLAGS += -DDEBUG_LEVEL=15

include $(TARGET)/Makefile

