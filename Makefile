include rules.mk

TARGET = arm/vexpress-a9
#TARGET = arm/virt

CFLAGS += -DDEBUG_LEVEL=3
#CFLAGS += -DDEBUG_LEVEL=67
#CFLAGS += -DDEBUG_LEVEL=79
#CFLAGS += -DDEBUG_LEVEL=95

include $(TARGET)/Makefile

.PHONY: test
test: 
	sh $(TARGET)/test

